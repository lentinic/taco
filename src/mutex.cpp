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

#include <basis/thread_util.h>
#include <taco/mutex.h>
#include <taco/fiber.h>

namespace taco
{
	mutex::mutex()
		:	m_locked(0)
	{}

	bool mutex::try_lock()
	{
		int expected = 0;
		return m_locked.compare_exchange_strong(expected, 1);
	}
	
	void mutex::lock()
	{
		int count = 0;
		while (!try_lock())
		{
			basis::cpu_pause();
			count++;
			if (count == 50)
			{
				fiber::yield();
				count = 0;
			}
		}
	}

	void mutex::unlock()
	{
		int locked = m_locked.load(std::memory_order_acquire);
		if (locked == 1)
		{
			m_locked.store(0, std::memory_order_release);
		}
	}
}
