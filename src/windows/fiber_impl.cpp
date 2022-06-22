/*
Chris Lentini
http://divergentcoder.io

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
#include <basis/thread_util.h>

#include "../fiber.h"
#include "../config.h"

namespace taco
{
	struct fiber
	{
		fiber_base		base;
		void *    		handle;
	};

	basis_thread_local static fiber * CurrentFiber = nullptr; 
	basis_thread_local static fiber * PreviousFiber = nullptr;
	basis_thread_local static fiber * ThreadFiber = nullptr;


	static void FiberHandoff(fiber * prev, fiber * cur)
	{
		if (prev->base.onExit)
		{
			auto tmp = prev->base.onExit;
			prev->base.onExit = nullptr;
			tmp();
		}
		if (cur->base.onEnter)
		{
			auto tmp = cur->base.onEnter;
			cur->base.onEnter = nullptr;
			tmp();
		}

		PreviousFiber = prev;
		CurrentFiber = cur;
	}

	static void __stdcall FiberMain(void * arg)
	{
		fiber * self = (fiber *) arg;
		BASIS_ASSERT(self != nullptr);
		FiberHandoff(CurrentFiber, self);
		self->base.fn();
	}

	void FiberInitializeThread()
	{
		BASIS_ASSERT(ThreadFiber == nullptr);

		ThreadFiber = new fiber;
		ConvertThreadToFiber(ThreadFiber);
		ThreadFiber->base.threadId = -1;
		ThreadFiber->base.data = nullptr;
		ThreadFiber->base.isBlocking = false;
		ThreadFiber->base.onEnter = ThreadFiber->base.onExit = nullptr;
		ThreadFiber->handle = GetCurrentFiber();
		CurrentFiber = ThreadFiber;
	}

	void FiberShutdownThread()
	{
		BASIS_ASSERT(CurrentFiber == ThreadFiber);
		delete ThreadFiber;
		ThreadFiber = nullptr;
	}
	
	fiber * FiberCreate(const fiber_fn & fn)
	{
		fiber * f = new fiber;
		f->base.fn = fn;
		f->base.threadId = -1;
		f->base.data = nullptr;
		f->base.isBlocking = false;
		f->base.onEnter = f->base.onExit = nullptr;
		f->handle = ::CreateFiber(FIBER_STACK_SIZE, &FiberMain, f);
		return f;
	}

	void FiberDestroy(fiber * f)
	{
		BASIS_ASSERT(CurrentFiber != f);
		::DeleteFiber(f->handle);
		delete f;
	}

	void FiberInvoke(fiber * f)
	{
		BASIS_ASSERT(f != nullptr);
		BASIS_ASSERT(f != CurrentFiber);

		fiber * self = CurrentFiber;
		SwitchToFiber(f->handle);
		FiberHandoff(CurrentFiber, self);
	}

	fiber * FiberCurrent()
	{
		return CurrentFiber;
	}

	fiber * FiberPrevious()
	{
		return PreviousFiber;
	}

	fiber * FiberRoot()
	{
		return ThreadFiber;
	}
}
