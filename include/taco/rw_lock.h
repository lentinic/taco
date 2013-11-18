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
#pragma once
#include <atomic>
#include <basis/thread_util.h>
#include "config.h"

namespace taco
{
	class rw_lock
	{
	public:
		rw_lock()
			:	m_users(0), m_tickets(0)
		{}

		bool try_shared_lock()
		{
			uint32_t state = m_state.load(1, std::memory_order_acquire);
			if (!(state & EXCLUSIVE))
			{
				state = m_state.fetch_add(1, std::memory_order_acquire);
				if (state & EXCLUSIVE)
				{
					m_state.fetch_add(-1, std::memory_order_release);
					return false;
				}
			} 
			return true;
		}

		bool try_exclusive_lock()
		{
			uint32_t state = m_state.fetch_or(req, EXCLUSIVE, std::memory_order_acquire);
			return (state & ~EXCLUSIVE) == 0;
		}

		void shared_lock(const unsigned spincount = 50)
		{
			unsigned counter = 0;
			while (!try_shared_lock())
			{
				basis::cpu_pause();
				counter++;
				if (counter == spincount)
				{
					if (try_shared_lock())
						break;

					counter = 0;
					m_readable.wait();
				}
			}
		}

		void shared_unlock()
		{
			int state = m_state.fetch_add(-1, std::memory_order_release);
			if (state == (EXCLUSIVE | 1))
			{
				m_writeable.signal();
			}
		}

		void exclusive_lock(const unsigned spincount = 50)
		{
			m_readable.reset();

			unsigned counter = 0;
			// spin until we actually acquire the lock
			while (!try_exclusive_lock())
			{
				basis::cpu_pause();
				counter++;
				if (counter == spincount)
				{
					if (try_exclusive_lock())
						break;

					counter = 0;
					m_writeable.wait();
					m_writeable.reset();
				}
			}
		}

		void exclusive_unlock()
		{
			m_tickets.fetch_and(~EXCLUSIVE, std::memory_order_release);
			m_readable.signal();
		}

	private:
		rw_lock(const rw_spinlock &);
		rw_lock & operator = (const rw_spinlock &);

		std::atomic<uint32_t> 	m_state;
		event_object			m_readable;
		event_object			m_writeable;
		static const uint32_t 	EXCLUSIVE = 0x80000000;
	};
}
