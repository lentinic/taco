/*
Copyright (c) 2013 Chris Lentini
http://divergentcoder.com

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to use, 
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANT*IES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include <utility>

#include <basis/assert.h>
#include <basis/thread_util.h>
#include <basis/chunk_queue.h>
#include <basis/string.h>

#include "scheduler_priv.h"
#include "config.h"
#include "fiber.h"
#include "profiler_priv.h"

namespace taco
{
	struct task_entry
	{
		task_fn			fn;
		basis::string	name;

		void operator () ()
		{
			TACO_PROFILER_EMIT(profiler::event_object::task, profiler::event_action::start, name);
			fn();
			TACO_PROFILER_EMIT(profiler::event_object::task, profiler::event_action::complete, name);
		}
	};

	typedef basis::chunk_queue<task_entry,basis::queue_access_policy::spmc> shared_task_queue_t;
	typedef basis::chunk_queue<task_entry,basis::queue_access_policy::mpsc> private_task_queue_t;

	typedef basis::chunk_queue<fiber *,basis::queue_access_policy::spmc> shared_fiber_queue_t;
	typedef basis::chunk_queue<fiber *,basis::queue_access_policy::mpsc> private_fiber_queue_t;

	struct scheduler_data
	{
		scheduler_data()
			:	sharedTasks(PUBLIC_TASKQ_CHUNK_SIZE),
				privateTasks(PRIVATE_TASKQ_CHUNK_SIZE),
				sharedFibers(PUBLIC_FIBERQ_CHUNK_SIZE),
				privateFibers(PRIVATE_FIBERQ_CHUNK_SIZE)
		{}

		shared_task_queue_t			sharedTasks;
		private_task_queue_t		privateTasks;
		shared_fiber_queue_t		sharedFibers;
		private_fiber_queue_t		privateFibers;
		
		std::thread 				thread;
		std::atomic_bool			exitRequested;
		std::condition_variable		wakeCondition;
		std::mutex					wakeMutex;
		std::vector<fiber*> 		inactive;
		std::atomic<uint32_t>		privateTaskCount;

		std::function<void()> *		onSuspend;

		uint32_t					threadId;
		bool 						isActive;
		bool 						isSignaled;
	};

	basis_thread_local static scheduler_data * Scheduler = nullptr;

	static std::atomic<uint32_t> GlobalSharedTaskCount;
	static scheduler_data * SchedulerList = nullptr;
	static uint32_t ThreadCount = 0;

	static void WorkerLoop();

	static bool HasTasks()
	{
		uint32_t shared_count = GlobalSharedTaskCount.load(std::memory_order_relaxed);
		uint32_t private_count = Scheduler->privateTaskCount.load(std::memory_order_relaxed);

		return (shared_count > 0) || (private_count > 0);
	}

	static bool GetPrivateTask(task_entry & out)
	{
		return Scheduler->privateTasks.pop_front(out);
	}

	static bool GetSharedTask(task_entry & out)
	{
		uint32_t start = Scheduler->threadId;
		uint32_t id = start;
		do
		{
			if (SchedulerList[id].sharedTasks.pop_front(out))
			{
				GlobalSharedTaskCount.fetch_sub(1, std::memory_order_relaxed);
				return true;
			}
			id = (id + 1) < ThreadCount ? (id + 1) : 0;
		} while (id != start);
		return false;
	}

	static void SignalScheduler(scheduler_data * s)
	{
		{
			std::unique_lock<std::mutex> lock(s->wakeMutex);
			s->isSignaled = true;
		}

		s->wakeCondition.notify_one();
	}

	static void AskForHelp(size_t count)
	{
		TACO_PROFILER_SCOPE("AskForHelp");

		uint32_t start = Scheduler->threadId;
		uint32_t id = (start + 1) < ThreadCount ? (start + 1) : 0;
		while (count > 0 && id != start)
		{
			if (SchedulerList[id].isActive)
			{
				SignalScheduler(SchedulerList + id);
				count--;
			}
			id = (id + 1) < ThreadCount ? (id + 1) : 0;
		}
	}

	static fiber * GetInactiveFiber()
	{
		size_t count = Scheduler->inactive.size();
		if (count > 0)
		{
			fiber * f = Scheduler->inactive[count - 1];
			Scheduler->inactive.pop_back();
			return f;
		}
		return FiberCreate(&WorkerLoop);
	}

	static fiber * GetSharedFiber()
	{
		fiber * ret = nullptr;
		uint32_t start = Scheduler->threadId;
		uint32_t id = start;
		do
		{
			if (SchedulerList[id].sharedFibers.pop_front(ret))
			{
				return ret;
			}
			id = (id + 1) < ThreadCount ? (id + 1) : 0;
		} while (id != start);
		return nullptr;
	}

	static fiber * GetNextScheduledFiber()
	{
		fiber * ret = nullptr;

		// TODO:  Something more intelligent...
		basis_thread_local static int source = 0;

		if (source == 0)
		{
			if (!Scheduler->privateFibers.pop_front(ret))
			{
				ret = GetSharedFiber();
			}
		}
		else
		{
			ret = GetSharedFiber();
			if (!ret)
			{
				Scheduler->privateFibers.pop_front(ret);
			}
		}
		source = (ret != nullptr) ? (source ^ 1) : source;

		return ret;
	}

	static fiber * GetNextFiber()
	{
		fiber * next = HasTasks() ? GetInactiveFiber() : GetNextScheduledFiber();
		next = (next == nullptr) ? GetInactiveFiber() : next;
		return next;
	}

	static void CheckForExitCondition()
	{
		if (Scheduler->exitRequested)
		{
			Scheduler->inactive.push_back(FiberCurrent());
			TACO_PROFILER_EMIT(taco::profiler::event_object::fiber, taco::profiler::event_action::complete);
			FiberInvoke(FiberRoot());
			BASIS_ASSERT_FAILED;
		}
	}

	static void FiberSwitchPoint()
	{
		CheckForExitCondition();

		fiber_base * prev = (fiber_base *) FiberPrevious();

		if (Scheduler->onSuspend)
		{
			std::function<void()> * fn = Scheduler->onSuspend;
			Scheduler->onSuspend = nullptr;
			(*fn)();
		}

		if (prev->command == scheduler_command::reschedule)
		{
			if (prev->threadId < 0)
			{
				Scheduler->sharedFibers.push_back((fiber *)prev);
			}
			else
			{
				Scheduler->privateFibers.push_back((fiber *)prev);
			}
		}
	}

	static void FiberSwitch(fiber * to)
	{
		TACO_PROFILER_EMIT(profiler::event_object::fiber, profiler::event_action::suspend);
		FiberInvoke(to);
		FiberSwitchPoint();
		TACO_PROFILER_EMIT(profiler::event_object::fiber, profiler::event_action::resume);
	}

	static bool WorkerIteration()
	{
		fiber * self = FiberCurrent();
		fiber_base * base = (fiber_base *) self;

		task_entry todo;
		if (GetPrivateTask(todo))
		{
			Scheduler->privateTaskCount.fetch_sub(1, std::memory_order_relaxed);
			base->threadId = Scheduler->threadId;
			base->data = nullptr;
			base->name = todo.name;
			todo();
			base->name = "";
			basis::strfree(todo.name);

			return true;
		}
		else if (GetSharedTask(todo))
		{
			base->threadId = -1;
			base->data = nullptr;
			base->name = todo.name;
			todo();
			base->name = "";
			basis::strfree(todo.name);

			return true;
		}
		else
		{
			fiber * next = GetNextScheduledFiber();
			if (next)
			{
				Scheduler->inactive.push_back(self);
				base->command = scheduler_command::none;
				FiberSwitch(next);
				return true;
			}
		}
		return false;
	}

	static void WorkerLoop()
	{
		TACO_PROFILER_EMIT(profiler::event_object::fiber, profiler::event_action::start);
		FiberSwitchPoint();
		for(;;)
		{
			CheckForExitCondition();

			if (!WorkerIteration())
			{
				std::unique_lock<std::mutex> lock(Scheduler->wakeMutex);
				if (!Scheduler->isSignaled)
				{
					TACO_PROFILER_EMIT(profiler::event_object::thread, profiler::event_action::suspend);
					Scheduler->wakeCondition.wait(lock);
					TACO_PROFILER_EMIT(profiler::event_object::thread, profiler::event_action::resume);
				}
				Scheduler->isSignaled = false;
			}
		}
	}

	static void ShutdownScheduler()
	{
		fiber * f = nullptr;

		while (Scheduler->privateFibers.pop_front(f))
		{
			FiberDestroy(f);
		}

		while (Scheduler->sharedFibers.pop_front(f))
		{
			FiberDestroy(f);
		}

		for (size_t i=0; i<Scheduler->inactive.size(); i++)
		{
			FiberDestroy(Scheduler->inactive[i]);
		}
		Scheduler->inactive.clear();

		Scheduler = nullptr;
	}

	void Initialize(int nthreads)
	{
		BASIS_ASSERT(ThreadCount == 0);

		ThreadCount = (nthreads <= 0) ? std::thread::hardware_concurrency() : nthreads;
		GlobalSharedTaskCount = 0;

		SchedulerList = new scheduler_data[ThreadCount];
		for (unsigned i=0; i<ThreadCount; i++)
		{
			SchedulerList[i].exitRequested = false;
			SchedulerList[i].threadId = i;
			SchedulerList[i].isActive = false;
			SchedulerList[i].isSignaled = false;
			SchedulerList[i].onSuspend = nullptr;
		}

		for (unsigned i=1; i<ThreadCount; i++)
		{
			SchedulerList[i].thread = std::thread([=]() -> void {
				Scheduler = SchedulerList + i;

				TACO_PROFILER_EMIT(profiler::event_object::thread, profiler::event_action::start);

				FiberInitializeThread();
				fiber * f = FiberCreate(&WorkerLoop);

				Scheduler->isActive = true;
				FiberInvoke(f);
				Scheduler->isActive = false;

				TACO_PROFILER_EMIT(profiler::event_object::thread, profiler::event_action::complete);

				ShutdownScheduler();
				FiberShutdownThread();
			});
		}

		Scheduler = SchedulerList;
		FiberInitializeThread();
	}

	void Initialize(const char * name, task_fn comain, int nthreads)
	{
		Initialize(nthreads);
		Schedule(name, [&]() -> void {
			comain();
			ExitMain();
		}, 0);
		EnterMain();
	}

	void Shutdown()
	{
		// Must be called from main thread and main thread must not
		// be in the scheduler loop
		BASIS_ASSERT(Scheduler == SchedulerList && !Scheduler->isActive);

		for (unsigned i=1; i<ThreadCount; i++)
		{
			SchedulerList[i].exitRequested = true;
			SignalScheduler(SchedulerList + i);
		}

		for (unsigned i=1; i<ThreadCount; i++)
		{
			SchedulerList[i].thread.join();
		}

		ShutdownScheduler();

		delete [] SchedulerList;
		SchedulerList = nullptr;
		ThreadCount = 0;

		FiberShutdownThread();
	}

	void EnterMain()
	{
		BASIS_ASSERT(Scheduler == SchedulerList);
		BASIS_ASSERT(!Scheduler->isActive);

		fiber * f = FiberCreate(&WorkerLoop);

		TACO_PROFILER_EMIT(profiler::event_object::thread, profiler::event_action::start);

		Scheduler->isActive = true;
		FiberInvoke(f);
		Scheduler->isActive = false;

		TACO_PROFILER_EMIT(profiler::event_object::thread, profiler::event_action::complete);

		Scheduler->exitRequested = false;
	}

	void ExitMain()
	{
		SchedulerList[0].exitRequested = true;
		SignalScheduler(SchedulerList);
	}

	void Schedule(const char * name, task_fn fn, uint32_t threadid)
	{
		BASIS_ASSERT(Scheduler != nullptr);
		
		if (threadid >=0 && threadid < ThreadCount)
		{
			scheduler_data * s = SchedulerList + threadid;
			s->privateTasks.push_back<task_entry>({ fn, basis::stralloc(name) });
			s->privateTaskCount.fetch_add(1, std::memory_order_relaxed);

			if (s != Scheduler)
			{
				SignalScheduler(s);
			}
		}
		else
		{
			BASIS_ASSERT(threadid == TACO_INVALID_THREAD_ID);
			
			Scheduler->sharedTasks.push_back<task_entry>({ fn, basis::stralloc(name) });
			uint32_t count = GlobalSharedTaskCount.fetch_add(1, std::memory_order_relaxed) + 1;
			if (count > 1 || !Scheduler->isActive)
			{
				AskForHelp(count);
			}
		}
	}

	void Switch()	
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		f->command = scheduler_command::reschedule;

		FiberSwitch(GetNextFiber());
	}

	void SetTaskLocalData(void * data)
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		f->data = data;
	}

	void * GetTaskLocalData()
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		return f->data;
	}

	const char * GetTaskName()
	{
		fiber_base  * f = (fiber_base *) FiberCurrent();
		return f->name;
	}

	void Suspend()
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		f->command = scheduler_command::none;
		FiberSwitch(GetNextFiber());
	}

	void Suspend(std::function<void()> on_suspend)
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		f->command = scheduler_command::none;
		Scheduler->onSuspend = &on_suspend;
		FiberSwitch(GetNextFiber());
	}
	
	void Resume(fiber * f)
	{
		fiber_base * base = (fiber_base *) f;
		if (base->threadId < 0)
		{
			Scheduler->sharedFibers.push_back(f);
		}
		else
		{
			BASIS_ASSERT(base->threadId >= 0 && (unsigned)base->threadId < ThreadCount);
			SchedulerList[base->threadId].privateFibers.push_back(f);
			SignalScheduler(SchedulerList + base->threadId);
		}
	}

	bool IsSchedulerThread()
	{
		return Scheduler != nullptr && Scheduler->isActive;
	}

	uint32_t GetSchedulerId()
	{
		return Scheduler ? Scheduler->threadId : INVALID_SCHEDULER_ID;
	}

	uint32_t GetThreadCount()
	{
		return ThreadCount;	
	}
}
