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
#include <condition_variable>

#include "fiber_scheduler.h"
#include "ring_buffer.h"
#include "thread_local.h"

#include <taco/assert.h>

namespace taco
{
	typedef ring_buffer<fiber,512,async_access_policy::mpsc> thread_queue_t;
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
		thread_queue_t					m_thread_fibers;
		std::condition_variable			m_cvar;
		std::mutex						m_mutex;
		std::atomic<scheduler_state>	m_state;
		std::atomic_bool				m_stop_requested;
	};

	static thread_local scheduler_id LocalScheduler = nullptr;
	static shared_queue_t SharedQ;

	scheduler_id CreateScheduler()
	{
		if (LocalScheduler != nullptr)
			return LocalScheduler;

		scheduler_t * scheduler = new scheduler_t;
		scheduler->m_state = scheduler_state::initialized;
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

		scheduler_state state = LocalScheduler->m_state.load(std::memory_order_acquire);
		TACO_ASSERT(state == scheduler_state::stopped);

		if (state == scheduler_state::stopped)
		{
			delete LocalScheduler;
			LocalScheduler = nullptr;
		}
	}

	void RunScheduler()
	{
		scheduler_t * self = LocalScheduler;
		TACO_ASSERT(self != nullptr);

		if (self == nullptr)
			return;

		self->m_state = scheduler_state::running;
		self->m_stop_requested = false;

		for(;;)
		{
			if (self->m_stop_requested)
				break;

			int work_done = 0;
			fiber todo;

			if (self->m_thread_fibers.pop_front(&todo))
			{
				work_done++;
				todo();
				if (todo.status() == fiber_status::active)
				{
					RunFiber(self, todo);
				}
			}

			if (SharedQ.pop_front(&todo))
			{
				work_done++;
				todo();
				if (todo.status() == fiber_status::active)
				{
					RunFiber(todo);
				}
			}
	
			if (work_done == 0)
			{
				std::unique_lock<std::mutex> lock(self->m_mutex);
				self->m_cvar.wait(lock);
			}
		}

		self->m_state = scheduler_state::stopped;
	}

	void StopScheduler(scheduler_id id)
	{
		scheduler_t * self = (id == nullptr) ? LocalScheduler : id;
		TACO_ASSERT(self != nullptr);
		if (self == nullptr)
			return;
	
		self->m_stop_requested = true;
		self->m_cvar.notify_one();
		return;
	}

	bool RunFiber(const fiber & f)
	{
		TACO_ASSERT(LocalScheduler != nullptr);
		if (LocalScheduler == nullptr)
			return false;

		SharedQ.push_back(f);
		LocalScheduler->m_cvar.notify_one();
		return true;
	}

	bool RunFiber(scheduler_id id, const fiber & f)
	{
		TACO_ASSERT(id != nullptr);
		if (id == nullptr)
			return false;

		id->m_thread_fibers.push_back(f);
		id->m_cvar.notify_one();
		return true;
	}
}
