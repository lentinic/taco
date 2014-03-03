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

#include <mutex>
#include <basis/assert.h>
#include <taco/condition.h>
#include <taco/scheduler.h>
#include "fiber.h"
#include "scheduler_priv.h"

namespace taco
{
	condition::condition()
	{}

	condition::~condition()
	{
		while (!m_waiting.empty())
		{
			fiber * f = m_waiting.front();
			FiberDestroy(f);
			m_waiting.pop_front();
		}
	}

	void condition::_wait(std::function<void()> on_suspend, std::function<void()> on_resume)
	{
		fiber * cur = FiberCurrent();
		BASIS_ASSERT(cur);

		{
			std::lock_guard<mutex> lock(m_mutex);
			m_waiting.push_back(cur);
		}

		Suspend(on_suspend, on_resume);
	}

	void condition::notify_one()
	{
		std::lock_guard<mutex> lock(m_mutex);
		fiber * f = m_waiting.front();
		m_waiting.pop_front();
		Resume(f);
	}

	void condition::notify_all()
	{
		std::lock_guard<mutex> lock(m_mutex);
		while (!m_waiting.empty())
		{
			fiber * f = m_waiting.front();
			m_waiting.pop_front();
			Resume(f);
		}
	}
}
