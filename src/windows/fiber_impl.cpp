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

#include <Windows.h>
#include <functional>
#include <atomic>

#include <basis/assert.h>
#include <basis/thread_local.h>

#include <taco/fiber.h>
#include "../fiber_impl.h"

#if !defined(FIBER_STACK_SZ)
#define FIBER_STACK_SZ 16384
#endif

namespace taco
{
	struct fiber_data
	{
		fiber_fn					fn;
		std::atomic_int				refCount;
		void *						handle;
		fiber_data *				parent;
		fiber_status				status;
	};

	static thread_local fiber_data * CurrentFiber = nullptr;
	static thread_local fiber_data * PreviousFiber = nullptr;
	static thread_local fiber_data * ThreadFiber = nullptr;

	static void FiberSwitch(fiber_data * to)
	{
		BASIS_ASSERT(to->handle != nullptr);

		PreviousFiber = CurrentFiber;
		CurrentFiber = to;

		SwitchToFiber(to->handle);
	}

	void __stdcall FiberMain(void * arg)
	{
		fiber_data * self = (fiber_data *) arg;
		BASIS_ASSERT(self != nullptr);
		BASIS_ASSERT(self == CurrentFiber);

		self->status = fiber_status::active;
		self->fn();
		self->status = fiber_status::complete;
		
		FiberYield();

		// We shouldn't re-enter the fiber after completion
		// If we do then assert and spin-yield
		BASIS_ASSERT_FAILED;
		for (;;)
		{
			FiberYield();
		}
	}

	void FiberInitializeThread()
	{
		BASIS_ASSERT(ThreadFiber == nullptr);

		ThreadFiber = new fiber_data;
		ConvertThreadToFiber(ThreadFiber);
		ThreadFiber->refCount = 1;
		ThreadFiber->handle = GetCurrentFiber();
		ThreadFiber->parent = nullptr;
		ThreadFiber->status = fiber_status::active;

		CurrentFiber = ThreadFiber;
	}

	void FiberShutdownThread()
	{
		BASIS_ASSERT(CurrentFiber == ThreadFiber);
		BASIS_ASSERT(ThreadFiber->refCount == 1);
		delete ThreadFiber;
		ThreadFiber = nullptr;
		CurrentFiber = nullptr;
	}

	fiber_data * FiberCreate(const fiber_fn & fn)
	{
		fiber_data * f = new fiber_data;
		f->fn = fn;
		f->refCount = 1;
		f->handle = ::CreateFiber(FIBER_STACK_SZ, &FiberMain, f);
		f->parent = nullptr;
		f->status = fiber_status::initialized;

		return f;
	}

	void FiberYieldTo(fiber_data * f)
	{
		BASIS_ASSERT(f != nullptr);

		f->parent = CurrentFiber->parent;
		CurrentFiber->parent = nullptr;

		FiberSwitch(f);
	}

	void FiberYield()
	{
		BASIS_ASSERT(CurrentFiber != nullptr);
		BASIS_ASSERT(CurrentFiber->parent != nullptr);
		
		fiber_data * to = CurrentFiber->parent;
		CurrentFiber->parent = nullptr;

		FiberSwitch(to);
	}

	void FiberInvoke(fiber_data * f)
	{
		BASIS_ASSERT(f != nullptr);
		
		f->parent = CurrentFiber;

		FiberSwitch(f);
	}

	fiber_status FiberStatus(fiber_data * f)
	{
		return f->status;
	}

	fiber_data * FiberCurrent()
	{
		return CurrentFiber;
	}

	void FiberAcquire(fiber_data * f)
	{
		BASIS_ASSERT(f != nullptr);
		int prev = f->refCount.fetch_add(1);
		BASIS_ASSERT(prev > 0);
	}

	void FiberRelease(fiber_data * f)
	{
		if (f == nullptr)
			return;

		int prev = f->refCount.fetch_sub(1);
		BASIS_ASSERT(prev > 0);
		if (prev == 1)
		{
			BASIS_ASSERT(f->status != fiber_status::active);
			::DeleteFiber(f->handle);
			delete f;
		}
	}
}
