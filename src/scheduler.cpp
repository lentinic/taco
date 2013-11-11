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

#include "fiber_scheduler.h"
#include "ring_buffer.h"
#include "thread_local.h"

#include <taco/assert.h>

namespace taco
{
	typedef ring_buffer<fiber,512,async_access_policy::mpmc> shared_queue_t;

	enum class scheduler_state
	{
		initialized,
		running,
		sleeping,
		stopped
	};

	struct scheduler_t
	{
		std::atomic<scheduler_state>	m_state;
		std::atomic_bool				m_stop_requested;
	};

	static thread_local scheduler_id		LocalScheduler = nullptr;
	static shared_queue_t					SharedQ;

	static std::mutex						SleepMutex;
	static std::condition_variable			SleepCondition;
	static std::list<scheduler_id>			SleepList;

	scheduler_id CreateScheduler()
	{
		if (LocalScheduler != nullptr)
			return LocalScheduler;

		fiber::initialize_thread();

		scheduler_t * scheduler = new scheduler_t;
		scheduler->m_state = scheduler_state::initialized;
		scheduler->m_stop_requested = false;
		LocalScheduler = scheduler;

		return scheduler;
	}

	scheduler_id CurrentScheduler()
	{
		return LocalScheduler;
	}

	void ShutdownScheduler()
	{
		if (LocalScheduler == nullptr)
			return;

		scheduler_state state = LocalScheduler->m_state;
		TACO_ASSERT(state == scheduler_state::stopped);
		if (state == scheduler_state::stopped)
		{
			fiber::shutdown_thread();
			delete LocalScheduler;
			LocalScheduler = nullptr;
		}
	}

	void RunScheduler()
	{
		scheduler_id self = LocalScheduler;
		TACO_ASSERT(self != nullptr);
		if (self == nullptr)
			return;

		self->m_state = scheduler_state::running;
		
		while (!self->m_stop_requested)
		{
			fiber todo;
			size_t work = RunSchedulerOnce();
			if (work == 0)
			{
				std::unique_lock<std::mutex> lock(SleepMutex);

				self->m_state = scheduler_state::sleeping;
				SleepList.push_back(self);
				SleepCondition.wait(lock);
				self->m_state = scheduler_state::running;
			}
		}

		self->m_state = scheduler_state::stopped;
	}

	bool RunSchedulerOnce()
	{
		scheduler_id self = LocalScheduler;
		TACO_ASSERT(self != nullptr);
		if (self == nullptr)
			return 0;

		size_t work_done = 0;
		fiber todo;
		if (SharedQ.pop_front(&todo))
		{
			work_done++;
			todo();
			if (todo.status() == fiber_status::active)
			{
				RunFiber(todo);
			}
		}

		return work_done != 0;
	}

	void StopScheduler(scheduler_id id)
	{
		scheduler_t * self = (id == nullptr) ? LocalScheduler : id;
		TACO_ASSERT(self != nullptr);
		if (self == nullptr)
			return;
	
		self->m_stop_requested = true;
		SleepCondition.notify_all();
		return;
	}

	bool RunFiber(const fiber & f)
	{
		if (!SharedQ.push_back(f))
			return false;
		
		SleepCondition.notify_one();
		return true;
	}
}
