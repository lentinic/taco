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
	typedef ring_buffer<fiber,512,async_access_policy::spmc> shared_queue_t;

	enum class thread_status
	{
		initialized,
		active,
		sleeping,
		death_pending,
		dead
	};

	struct scheduler_info
	{
		thread_queue_t				m_thread_fibers[2];
		shared_queue_t				m_shared_fibers[2];
		std::thread					m_thread;
		std::condition_variable		m_cvar;
		std::mutex					m_mutex;
		std::atomic<thread_status>	m_status;
		std::atomic<int>			m_qid;
		int							m_id;
		int							m_scheduled;
	};

	static scheduler_info * ThreadTable = nullptr;
	static int ThreadCount = 0;
	static thread_local scheduler_info * CurrentScheduler = nullptr;

	void SchedulerMain(scheduler_info * self)
	{
		fiber::initialize_thread();

		CurrentScheduler = self;
		int loops = 0;

		while (self->m_status != thread_status::death_pending)
		{
			self->m_status = thread_status::active;

			bool did_proc = false;
			fiber current;

			int qid = self->m_qid.load(std::memory_order_relaxed);
			int next = qid ^ 1;

			while (self->m_thread_fibers[qid].pop_front(&current))
			{
				did_proc = true;
				current();
				if (current.status() == fiber_status::active)
				{
					self->m_thread_fibers[next].push_back(current);
				}
			}

			while (self->m_shared_fibers[qid].pop_front(&current))
			{
				did_proc = true;
				current();
				if (current.status() == fiber_status::active)
				{
					self->m_shared_fibers[next].push_back(current);
				}
			}

			int base_id = self->m_id;
			int cur_id = (base_id + 1) == ThreadCount ? 0 : (base_id + 1);
			while (cur_id != base_id)
			{
				qid = ThreadTable[cur_id].m_qid.load(std::memory_order_relaxed);
				while (ThreadTable[cur_id].m_shared_fibers[qid].pop_front(&current))
				{
					did_proc = true;
					current();
					if (current.status() == fiber_status::active)
					{
						self->m_shared_fibers[next].push_back(current);
					}
				}
				cur_id = (cur_id + 1) == ThreadCount ? 0 : (cur_id + 1);
			}

			self->m_qid.store(next, std::memory_order_relaxed);
			self->m_scheduled = 0;
			loops++;

			if (!did_proc && loops == 2)
			{
				self->m_status = thread_status::sleeping;
				std::unique_lock<std::mutex> lock(self->m_mutex);
				self->m_cvar.wait(lock);
				lock.unlock();
				loops = 0;
			}
		}

		self->m_status = thread_status::dead;
	}

	void Initialize(int nthreads)
	{
		nthreads = (nthreads == -1) ? std::thread::hardware_concurrency() : nthreads;
		nthreads = std::max(1, nthreads);
		ThreadCount = nthreads;

		ThreadTable = new scheduler_info[nthreads];
		ThreadTable[0].m_status = thread_status::initialized;
		ThreadTable[0].m_id = 0;
		ThreadTable[0].m_qid = 0;
		ThreadTable[0].m_scheduled = 0;

		CurrentScheduler = ThreadTable;

		for (int i=1; i<nthreads; i++)
		{
			ThreadTable[i].m_status = thread_status::initialized;
			ThreadTable[i].m_id= i;
			ThreadTable[i].m_qid = 0;
			ThreadTable[i].m_scheduled = 0;
		}

		for (int i=1; i<nthreads; i++)
		{
			ThreadTable[i].m_thread = std::thread(&SchedulerMain, ThreadTable + i);
		}
	}
	
	void EnterScheduler()
	{
		TACO_ASSERT(CurrentScheduler != nullptr);
		TACO_ASSERT(CurrentScheduler->m_status == thread_status::initialized);

		SchedulerMain(CurrentScheduler);
	}

	bool Schedule(const fiber & f, int affinity)
	{
		TACO_ASSERT(CurrentScheduler != nullptr);

		int nid = CurrentScheduler->m_qid.load(std::memory_order_relaxed) ^ 1;
		if (affinity == -1 && CurrentScheduler->m_shared_fibers[nid].push_back(f))
		{
			int count = CurrentScheduler->m_scheduled++;
			if (CurrentScheduler->m_status != thread_status::active || count >= 1)
			{
				int base_id = CurrentScheduler->m_id;
				int cur_id = (base_id + 1) == ThreadCount ? 0 : (base_id + 1);
				while (cur_id != base_id && count > 0)
				{
					if (ThreadTable[cur_id].m_status == thread_status::sleeping)
						ThreadTable[cur_id].m_cvar.notify_one();
					count--;
					cur_id = (cur_id + 1) == ThreadCount ? 0 : (cur_id + 1);
				}
			}

			return true;
		}

		TACO_ASSERT(affinity >= 0 && affinity < ThreadCount);

		nid = ThreadTable[affinity].m_qid.load(std::memory_order_relaxed) ^ 1;
		if (ThreadTable[affinity].m_thread_fibers[nid].push_back(f))
		{
			if (ThreadTable[affinity].m_status == thread_status::sleeping)
			{
				ThreadTable[affinity].m_cvar.notify_one();
			}
			return true;
		}

		return false;
	}
}
