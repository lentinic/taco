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

#pragma once

#include <Windows.h>
#include <functional>
#include <atomic>

#include <taco/assert.h>

#include "../thread_local.h"

#if !defined(FIBER_STACK_SZ)
#define FIBER_STACK_SZ 16384
#endif

namespace taco
{
	struct fiber_id_t
	{
		std::function<void()>		m_fn;
		std::atomic_int				m_refcount;
		void *						m_handle;
		fiber_id_t *				m_parent;
	};

	static thread_local fiber_id_t * CurrentFiber = nullptr;
	static thread_local fiber_id_t * ThreadFiber = nullptr;

	void __stdcall FiberMain(void * arg)
	{
		fiber_id_t * self = (fiber_id_t *) arg;
		TACO_ASSERT(self != nullptr);
		self->m_fn();
	}

	void FiberInitializeThread()
	{
		TACO_ASSERT(ThreadFiber == nullptr);
		ThreadFiber = new fiber_id_t;
		ThreadFiber->m_refcount = 1;
		ConvertThreadToFiber(ThreadFiber);
		ThreadFiber->m_handle = GetCurrentFiber();
		CurrentFiber = ThreadFiber;
	}

	void FiberShutdownThread()
	{
		TACO_ASSERT(CurrentFiber == ThreadFiber);
		TACO_ASSERT(ThreadFiber->m_refcount == 1);
		delete ThreadFiber;
		ThreadFiber = nullptr;
		CurrentFiber = nullptr;
	}

	fiber_id_t * FiberCreate(const std::function<void()> & fn)
	{
		fiber_id_t * f = new fiber_id_t;
		f->m_fn = fn;
		f->m_refcount = 1;
		f->m_handle = ::CreateFiber(FIBER_STACK_SZ, &FiberMain, f);

		return f;
	}

	void FiberYieldTo(fiber_id_t * f)
	{
		TACO_ASSERT(f != nullptr);
		TACO_ASSERT(f->m_handle != nullptr);
		TACO_ASSERT(f->m_parent == nullptr);

		f->m_parent = CurrentFiber->m_parent;
		CurrentFiber->m_parent = nullptr;
		SwitchToFiber(f->m_handle);
	}

	void FiberYield()
	{
		TACO_ASSERT(CurrentFiber != nullptr);
		TACO_ASSERT(CurrentFiber->m_parent != nullptr);
		
		fiber_id_t * to = CurrentFiber->m_parent;
		CurrentFiber->m_parent = nullptr;
		SwitchToFiber(to->m_handle);
	}

	void FiberInvoke(fiber_id_t * f)
	{
		TACO_ASSERT(f != nullptr);
		TACO_ASSERT(f->m_handle != nullptr);
		TACO_ASSERT(f->m_parent == nullptr);
		
		f->m_parent = CurrentFiber;
		SwitchToFiber(f->m_handle);
	}

	void FiberAcquire(fiber_id_t * f)
	{
		TACO_ASSERT(f != nullptr);
		int prev = f->m_refcount.fetch_add(1);
		TACO_ASSERT(prev != 0);
	}

	void FiberRelease(fiber_id_t * f)
	{
		if (f == nullptr)
			return;

		int prev = f->m_refcount.fetch_sub(1);
		TACO_ASSERT(prev > 0);
		if (prev == 1)
		{
			::DeleteFiber(f->m_handle);
			delete f;
		}
	}
}
