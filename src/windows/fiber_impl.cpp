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

#if !defined(FIBER_STACK_SZ)
#define FIBER_STACK_SZ 16384
#endif

namespace taco
{
	struct fiber
	{
		fiber_fn					fn;
		void *						handle;
		fiber *						parent;
	};

	static basis_thread_local fiber_data * CurrentFiber = nullptr;
	static basis_thread_local fiber_data * ThreadFiber = nullptr;


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
		ThreadFiber->handle = GetCurrentFiber();
		ThreadFiber->parent = nullptr;

		CurrentFiber = ThreadFiber;
	}

	void FiberShutdownThread()
	{
		BASIS_ASSERT(CurrentFiber == ThreadFiber);
		delete ThreadFiber;
		ThreadFiber = nullptr;
		CurrentFiber = nullptr;
	}

	fiber * FiberCreate(const fiber_fn & fn)
	{
		fiber_data * f = new fiber_data;
		f->fn = fn;
		f->handle = ::CreateFiber(FIBER_STACK_SZ, &FiberMain, f);
		f->parent = nullptr;

		return f;
	}

	void FiberDestroy(fiber * f)
	{
		BASIS_ASSERT(CurrentFiber != f);
		::DeleteFiber(f->handle);
		delete f;
	}

	void FiberYieldTo(fiber * f)
	{
		BASIS_ASSERT(f != nullptr);
		if (f == CurrentFiber)
			return;

		f->parent = CurrentFiber->parent;
		CurrentFiber->parent = nullptr;

		SwitchToFiber(f->handle);
	}

	void FiberYield()
	{
		BASIS_ASSERT(CurrentFiber != nullptr);
		BASIS_ASSERT(CurrentFiber->parent != nullptr);

		fiber * to = CurrentFiber->parent;
		CurrentFiber->parent = nullptr;

		SwitchToFiber(to->handle);
	}

	void FiberInvoke(fiber * f)
	{
		BASIS_ASSERT(f != nullptr);
		if (f == CurrentFiber)
			return;
		
		f->parent = CurrentFiber;
		SwitchToFiber(f->handle);
	}

	fiber * FiberCurrent()
	{
		return CurrentFiber;
	}
}
