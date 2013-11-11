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
#include <list>
#include <thread>
#include <condition_variable>
#include <basis/thread_local.h>
#include <basis/assert.h>

#include "taco/scheduler.h"
#include "ring_buffer.h"

namespace taco
{
	typedef ring_buffer<fiber,512,async_access_policy::spmc> shared_queue_t;
	typedef ring_buffer<fiber,512,async_access_policy::mpsc> private_queue_t;

	enum class scheduler_state
	{
		initialized,
		running,
		sleeping,
		stopping,
		stopped
	};

	struct scheduler
	{
		shared_queue_t					sharedFibers;
		private_queue_t					privateFibers;

		std::mutex 						schedLock;
		std::condition_variable 		hasWork;

		std::atomic<scheduler_state>	state;
		scheduler *						next;
		scheduler *						prev;
	};

	static thread_local scheduler *									LocalScheduler = nullptr;
	static scheduler *												SchedulerList = nullptr;
	static spinlock													ListLock;

	void InitializeScheduler()
	{
		BASIS_ASSERT(LocalScheduler == nullptr);
		if (LocalScheduler != nullptr)
			return;

		fiber::initialize_thread();

		scheduler * s = new scheduler;
		s->state = scheduler_state::initialized;
		s->isStopRequested = false;
		LocalScheduler = s;

		ListLock.lock();
		scheduler * list = SchedulerList;
		if (list == nullptr)
		{
			s->next = s->prev = s;
		}
		else
		{
			s->prev = list->prev;
			list->prev->next = s;

			s->next = list;
			list->prev = s;
		}
		SchedulerList = s;
		ListLock.unlock();
	}

	void ShutdownScheduler()
	{
		scheduler * self = LocalScheduler;
		BASIS_ASSERT(self != nullptr);
		if (self == nullptr)
			return;

		scheduler_state state = self->state;
		BASIS_ASSERT(state == scheduler_state::stopped);
		if (state == scheduler_state::stopped)
		{
			ListLock.lock();
			self->prev->next = self->next;
			self->next->prev = self->prev;
			if (self == SchedulerList)
			{
				SchedulerList = (self->prev == self) ? nullptr : self->prev;
			}
			ListLock.unlock();

			fiber::shutdown_thread();
			delete self;
			LocalScheduler = nullptr;
		}
	}

	static void WakeOtherSchedulers(scheduler * self, size_t count)
	{
		ListLock.lock();
		scheduler * other = self->next;
		while (count > 1 && other != self)
		{
			if (other->state == scheduler_state::sleeping)
			{
				other->hasWork.notify_one();
				count--;
			}

			other = other->next;
		}
		ListLock.unlock();
	}

	static bool PerformWork(scheduler * self, bool steal = true)
	{
		fiber todo;
		if (!self->sharedFibers.pop_front(&todo))
		{
			if (!steal)
				return false;

			ListLock.lock();
			scheduler * other = self->next;
			while (other != self && !other->sharedFibers.pop_front(&todo))
			{
				other = other->next;
			}	
			ListLock.unlock();

			if (!todo)
				return false;
		}


		todo();
		if (todo.status() == fiber_status::inactive)
		{
			ScheduleFiber(todo);
		}
		return true;
	}

	void EnterScheduler()
	{
		scheduler * self = LocalScheduler;
		BASIS_ASSERT(self != nullptr);
		if (self == nullptr)
			return;

		self->state = scheduler_state::running;
		std::unique_lock<std::mutex> lock(self->schedLock);

		while (self->state != scheduler_state::stopping)
		{
			if (!PerformWork())
			{
				self->state = scheduler_state::sleeping;
				self->hasWork.wait(lock);
				self->state = scheduler_state::running;
			}
		}

		self->state = scheduler_state::stopped;
	}

	void ExitScheduler()
	{
		scheduler * self = LocalScheduler;
		BASIS_ASSERT(self != nullptr);
		if (self == nullptr)
			return;
	
		self->state = scheduler_state::stopping;
	}

	void ScheduleFiber(const fiber & f)
	{
		scheduler * self = LocalScheduler;
		BASIS_ASSERT(self != nullptr);
		if (self == nullptr)
			return false;

		unsigned int count = 0;
		while (!self->sharedFibers.push_back(f) && PerformWork(self, false))
		{
			count++;
			if (count == 512)
				break;
		}

		if (count == 512)
			return false;
		
		size_t nfibers = self->sharedFibers.count();
		if (nfibers == 1)
			return true;

		WakeOtherSchedulers(self, nfibers - 1);

		return true;
	}
}
