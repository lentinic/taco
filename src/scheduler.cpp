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
#include <thread>
#include <mutex>
#include <condition_variable>

#include <basis/thread_local.h>
#include <basis/assert.h>

#include "taco/scheduler.h"
#include "ring_buffer.h"
#include "shared_mutex.h"

namespace taco
{
	typedef ring_buffer<fiber,512,async_access_policy::spmc> shared_queue_t;
	typedef ring_buffer<fiber,512,async_access_policy::mpsc> private_queue_t;

	enum class scheduler_state
	{
		initialized,
		running,
		sleeping,
		stopped
	};

	struct scheduler
	{
		shared_queue_t					sharedFibers;
		private_queue_t					privateFibers;

		std::mutex 						schedLock;
		std::condition_variable 		hasWork;

		std::atomic<scheduler_state>	state;
		std::atomic_bool				stopRequested;
		scheduler_id					next;
		scheduler_id					prev;
	};

	static thread_local scheduler_id	LocalScheduler = nullptr;
	static scheduler_id					SchedulerList = nullptr;
	static shared_mutex					ListMutex;

	scheduler_id CreateScheduler()
	{
		scheduler_id s = new scheduler;
		s->state = scheduler_state::initialized;
		s->stopRequested = false;

		ListMutex.lock();
		scheduler_id list = SchedulerList;
		s->next = list == nullptr ? s : list;
		s->prev = list == nullptr ? s : list->prev;
		if (list)
		{
			list->prev->next = s;
			list->prev = s;
		}
		SchedulerList = s;
		ListMutex.unlock();

		return s;
	}

	void DestroyScheduler(scheduler_id id)
	{
		BASIS_ASSERT(id->state == scheduler_state::stopped);

		ListMutex.lock();
		id->prev->next = id->next;
		id->next->prev = id->prev;
		if (SchedulerList == id)
		{
			scheduler_id next = SchedulerList->next;
			SchedulerList = next == id ? nullptr : next;
		}
		ListMutex.unlock();

		delete id;
	}

	static void WakeOtherSchedulers(scheduler_id id, size_t count)
	{
		ListMutex.lock_shared();
		scheduler * other = id->next;
		while (count > 0 && other != id)
		{
			if (other->state == scheduler_state::sleeping)
			{
				other->hasWork.notify_one();
				count--;
			}

			other = other->next;
		}
		ListMutex.unlock_shared();
	}

	static bool PerformWork(scheduler_id id)
	{
		fiber todo;
		if (!id->sharedFibers.pop_front(&todo))
		{
			ListMutex.lock_shared();
			scheduler_id other = id->next;
			while (other != id && !other->sharedFibers.pop_front(&todo))
			{
				other = other->next;
			}	
			ListMutex.unlock_shared();

			if (!todo)
				return false;
		}

		todo();

		if (todo.status() == fiber_status::active)
		{
			ScheduleFiber(todo);
		}

		return true;
	}

	void EnterScheduler(scheduler_id id)
	{
		BASIS_ASSERT(LocalScheduler == nullptr);
		if (LocalScheduler != nullptr)
			return;
		LocalScheduler = id;

		id->state = scheduler_state::running;

		while (!id->stopRequested)
		{
			if (!PerformWork(id))
			{
				std::unique_lock<std::mutex> lock(id->schedLock);
				if (id->stopRequested)
				{
					break;
				}

				id->state = scheduler_state::sleeping;
				id->hasWork.wait(lock);
				id->state = scheduler_state::running;
			}
		}

		id->state = scheduler_state::stopped;
	}

	void ExitScheduler(scheduler_id id)
	{
		std::unique_lock<std::mutex> lock(id->schedLock);
		id->stopRequested = true;
		id->hasWork.notify_one();
	}

	void ScheduleFiber(const fiber & f, scheduler_id id)
	{
		scheduler_id s = (id == nullptr) ? LocalScheduler : id;
		BASIS_ASSERT(s != nullptr);
		if (s == nullptr)
			return;

		s->sharedFibers.push_back(f);
		s->hasWork.notify_one();

		size_t nfibers = 2;//s->sharedFibers.count();
		if (nfibers == 1)
			return;

		WakeOtherSchedulers(s, nfibers - 1);
	}
}
