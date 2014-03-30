/*
Copyright (c) 2014 Chris Lentini
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

#include <basis/assert.h>
#include <basis/thread_util.h>
#include <taco/scheduler.h>
#include <taco/shared_mutex.h>
#include "scheduler_priv.h"
#include "config.h"

#define EXCLUSIVE 0x80000000

namespace taco
{
	shared_mutex::shared_mutex()
		: m_state(0)
	{}

	bool shared_mutex::try_lock_shared()
	{
		uint32_t state = m_state.fetch_add(1);
		if (!!(state & EXCLUSIVE))
		{
			state = m_state.fetch_sub(1);
			return false;
		}
		return true;
	}
	
	void shared_mutex::lock_shared()
	{
		unsigned counter = 0;
		while (!try_lock_shared())
		{
			counter++;
			if (counter == MUTEX_SPIN_COUNT)
			{
				Switch();
				counter = 0;
			}
			else
			{
				basis::cpu_yield();
			}
		}
	}

	void shared_mutex::unlock_shared()
	{
		uint32_t state = m_state.fetch_sub(1);
		BASIS_ASSERT((state & (~EXCLUSIVE)) != 0);
	}

	bool shared_mutex::try_lock()
	{
		uint32_t expected = 0;
		return m_state.compare_exchange_strong(expected, EXCLUSIVE);
	}

	void shared_mutex::lock()
	{
		// Try to acquire the lock, but if we can't try and prevent further shared locks
		uint32_t state = m_state.fetch_or(EXCLUSIVE);
		unsigned counter = 0;

		while (state != 0)
		{
			if ((state & EXCLUSIVE) == 0)
			{
				// we own the next lock, wait for shared locks to release
				while ((m_state.load() & (~EXCLUSIVE)) != 0)
				{
					counter++;
					if (counter == MUTEX_SPIN_COUNT)
					{
						Switch();
						counter = 0;
					}
					else
					{
						basis::cpu_yield();
					}
				}	
				return;
			}

			// someone else owns the lock or is waiting on the lock

			counter++;
			if (counter == MUTEX_SPIN_COUNT)
			{
				Switch();
				counter = 0;
			}
			else
			{
				basis::cpu_yield();
			}

			state = m_state.fetch_or(EXCLUSIVE);
		}
	}

	void shared_mutex::unlock()
	{
		uint32_t state = m_state.fetch_and(~EXCLUSIVE);
		BASIS_ASSERT(!!(state & EXCLUSIVE));
	}
}
