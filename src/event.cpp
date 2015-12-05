/*
Chris Lentini
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

#include <mutex>
#include <basis/assert.h>
#include <taco/event.h>
#include <taco/scheduler.h>
#include "fiber.h"
#include "scheduler_priv.h"
#include "profiler_priv.h"

namespace taco
{
	event::event()
	{}

	event::~event()
	{
		while (!m_waiting.empty())
		{
			fiber * f = m_waiting.front();
			FiberDestroy(f);
			m_waiting.pop_front();
		}
	}

	void event::wait()
	{
		BASIS_ASSERT(IsSchedulerThread());
		TACO_PROFILER_LOG("event::wait <%p>", this);

		fiber * cur = FiberCurrent();
		BASIS_ASSERT(cur);

		if (m_ready.load(std::memory_order_relaxed))
		{
			return;
		}

		m_mutex.lock();
		if (m_ready.load(std::memory_order_relaxed))
		{
			m_mutex.unlock();
			return;
		}

		Suspend([&]() -> void {
			m_waiting.push_back(cur);
			m_mutex.unlock();
		});
	}

	void event::signal()
	{
		BASIS_ASSERT(IsSchedulerThread());
		TACO_PROFILER_LOG("event::signal <%p>", this);
		
		m_mutex.lock();
		m_ready = true;
		m_mutex.unlock();
		
		while (!m_waiting.empty())
		{
			auto f = m_waiting.front();
			Resume(f);
			m_waiting.pop_front();
		}
	}

	void event::reset()
	{
		BASIS_ASSERT(m_waiting.empty());
		m_ready = false;
	}
}
