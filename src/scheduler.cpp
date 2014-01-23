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

#include <basis/assert.h>
#include <basis/thread_local.h>
#include <basis/thread_util.h>

#include "scheduler_priv.h"
#include "config.h"
#include "ring_buffer.h"
#include "fiber.h"

namespace taco
{
	typedef ring_buffer<task_fn,TACO_SHARED_TASK_QUEUE_SIZE,async_access_policy::mpmc> shared_task_queue_t;
	typedef ring_buffer<task_fn,TACO_PRIVATE_TASK_QUEUE_SIZE,async_access_policy::mpmc> private_task_queue_t;
	typedef ring_buffer<fiber *,TACO_SHARED_FIBER_QUEUE_SIZE,async_access_policy::mpmc> shared_fiber_queue_t;
	typedef ring_buffer<fiber *,TACO_PRIVATE_FIBER_QUEUE_SIZE,async_access_policy::mpmc> private_fiber_queue_t;

	struct scheduler_data
	{
		shared_task_queue_t			sharedTasks;
		private_task_queue_t		privateTasks;
		shared_fiber_queue_t		sharedFibers;
		private_fiber_queue_t		privateFibers;
		
		std::thread 				thread;
		std::atomic_bool			exitRequested;
		std::condition_variable		wakeCondition;
		std::mutex					wakeMutex;
		std::vector<fiber*> 		inactive;
		uint32_t					threadId;
		bool 						isActive;
	};

	basis_thread_local static scheduler_data * Scheduler = nullptr;

	static std::atomic<uint32_t> GlobalSharedTaskCount;
	static std::atomic<uint32_t> AwakeThreadCount;
	static scheduler_data * SchedulerList = nullptr;
	static uint32_t ThreadCount = 0;

	static void WorkerLoop();

	static bool HasTasks()
	{
		uint32_t shared = GlobalSharedTaskCount.load(std::memory_order_acquire);
		return Scheduler->privateTasks.count() > 0 || shared > 0;
	}

	static bool GetPrivateTask(task_fn * out)
	{
		return Scheduler->privateTasks.pop_front(out);
	}

	static bool GetSharedTask(task_fn * out)
	{
		uint32_t start = Scheduler->threadId;
		uint32_t id = start;
		do
		{
			if (SchedulerList[id].sharedTasks.pop_front(out))
			{
				GlobalSharedTaskCount.fetch_sub(1, std::memory_order_release);
				return true;
			}
			id = (id + 1) < ThreadCount ? (id + 1) : 0;
		} while (id != start);
		return false;
	}

	static void SignalScheduler(scheduler_data * s)
	{
		s->wakeCondition.notify_one();
	}

	static void AskForHelp(size_t count)
	{
		uint32_t start = Scheduler->threadId;
		uint32_t id = (start + 1) < ThreadCount ? (start + 1) : 0;
		while (count > 0 && id != start)
		{
			SignalScheduler(SchedulerList + id);
			count--;
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
			if (SchedulerList[id].sharedFibers.pop_front(&ret))
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
			if (!Scheduler->privateFibers.pop_front(&ret))
			{
				ret = GetSharedFiber();
			}
		}
		else
		{
			ret = GetSharedFiber();
			if (!ret)
			{
				Scheduler->privateFibers.pop_front(&ret);
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

	static void FiberSwitchPoint()
	{
		fiber_base * prev = (fiber_base *) FiberPrevious();
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

	static void WorkerLoop()
	{
		FiberSwitchPoint();

		fiber * self = FiberCurrent();
		fiber_base * base = (fiber_base *) self;

		while (!Scheduler->exitRequested)
		{
			task_fn todo;
			if (GetPrivateTask(&todo))
			{
				base->threadId = -1;
				todo();
			}
			else if (GetSharedTask(&todo))
			{
				base->threadId = Scheduler->threadId;
				todo();
			}
			else
			{
				fiber * next = GetNextScheduledFiber();
				if (next)
				{
					Scheduler->inactive.push_back(self);
					base->command = scheduler_command::none;
					FiberInvoke(next);
					FiberSwitchPoint();
				}
				else
				{
					//AwakeThreadCount--;
					//Sleep(0);
					//std::unique_lock<std::mutex> lock(Scheduler->wakeMutex);
					//Scheduler->wakeCondition.wait(lock);
					//AwakeThreadCount++;
				}
			}
		}

		Scheduler->inactive.push_back(self);
		FiberInvoke(FiberRoot());

		BASIS_ASSERT_FAILED;
	}

	static void ShutdownScheduler()
	{
		fiber * f = nullptr;
		while (Scheduler->privateFibers.pop_front(&f))
		{
			FiberDestroy(f);
		}
		while (Scheduler->sharedFibers.pop_front(&f))
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
		AwakeThreadCount = 0;
		GlobalSharedTaskCount = 0;

		SchedulerList = new scheduler_data[ThreadCount];
		for (unsigned i=0; i<ThreadCount; i++)
		{
			SchedulerList[i].exitRequested = false;
			SchedulerList[i].threadId = i;
			SchedulerList[i].isActive = false;
		}

		for (unsigned i=1; i<ThreadCount; i++)
		{
			SchedulerList[i].thread = std::thread([=]() -> void {
				AwakeThreadCount++;
				FiberInitializeThread();
				Scheduler = SchedulerList + i;

				fiber * f = FiberCreate(&WorkerLoop);

				Scheduler->isActive = true;
				FiberInvoke(f);
				Scheduler->isActive = false;

				ShutdownScheduler();
				FiberShutdownThread();

				AwakeThreadCount--;
			});
		}

		Scheduler = SchedulerList;
		FiberInitializeThread();
	}

	void Shutdown()
	{
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

		AwakeThreadCount++;
		Scheduler->isActive = true;
		FiberInvoke(f);
		Scheduler->isActive = false;
		AwakeThreadCount--;

		Scheduler->exitRequested = false;
	}

	void ExitMain()
	{
		SchedulerList[0].exitRequested = true;
		SignalScheduler(SchedulerList);
	}

	void Schedule(task_fn t, int threadid)
	{
		if (threadid >= 0 && (unsigned)threadid < ThreadCount)
		{
			BASIS_ASSERT(SchedulerList != nullptr);
			scheduler_data * s = SchedulerList + threadid;
			s->privateTasks.push_back(t);
			if (s != Scheduler)
			{
				SignalScheduler(s);
			}
		}
		else
		{
			BASIS_ASSERT(Scheduler != nullptr);
			Scheduler->sharedTasks.push_back(t);
			uint32_t count = GlobalSharedTaskCount.fetch_add(1, std::memory_order_release);
			uint32_t awake = AwakeThreadCount.load(std::memory_order_acquire);
			if (count > awake)
			{
				AskForHelp(count - awake);
			}
		}
	}

	void Yield()	
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		f->command = scheduler_command::reschedule;
		FiberInvoke(GetNextFiber());
		FiberSwitchPoint();
	}

	void Suspend()
	{
		fiber_base * f = (fiber_base *) FiberCurrent();
		f->command = scheduler_command::none;
		FiberInvoke(GetNextFiber());
		FiberSwitchPoint();
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
}
