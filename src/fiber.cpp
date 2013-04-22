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
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <taco/fiber.h>
#include <taco/assert.h>
#include <atomic>

#include "fiber_impl.h"

namespace taco
{
	fiber::fiber()
		: m_id(nullptr)
	{
	}

	fiber::fiber(const std::function<void()> & fn)
		: m_id(FiberCreate(fn))
	{
	}

	fiber::fiber(const fiber & other)
	{
		FiberAcquire(other.m_id);
		m_id = other.m_id;
	}

	fiber & fiber::operator = (const fiber & other)
	{
		FiberRelease(m_id);
		FiberAcquire(other.m_id);
		m_id = other.m_id;
		return *this;
	}

	fiber::~fiber()
	{
		FiberRelease(m_id);
		m_id = nullptr;
	}

	void fiber::initialize_thread()
	{
		FiberInitializeThread();
	}

	void fiber::shutdown_thread()
	{
		FiberShutdownThread();
	}

	void fiber::yield_to(const fiber & other)
	{
		FiberYieldTo(other.m_id);
	}

	void fiber::yield()
	{
		FiberYield();
	}

	void fiber::run(fiber_id id)
	{
		FiberInvoke(id);
	}

	fiber_id fiber::current()
	{
		return CurrentFiber;
	}
}
