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
#include <ucontext.h>
#include <setjmp.h>

#include <functional>
#include <atomic>

#include <basis/assert.h>
#include <basis/thread_util.h>

#include "../fiber.h"
#include "../config.h"

// using technique detailed at http://www.1024cores.net/home/lock-free-algorithms/tricks/fibers

namespace taco
{
	struct fiber
	{
		fiber_base				base;
		ucontext_t				ctx;
		jmp_buf   				jmp;
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

	static void FiberMain(void * arg)
	{
		fiber * self = (fiber *) arg;
		BASIS_ASSERT(self != nullptr);

		if (_setjmp(self->jmp) == 0)
		{
			swapcontext(&self->ctx, &CurrentFiber->ctx);
		}
		else
		{
			BASIS_ASSERT_FAILED;
		}

		FiberHandoff(CurrentFiber, self);

		self->base.fn();
	}

	void FiberInitializeThread()
	{
		BASIS_ASSERT(ThreadFiber == nullptr);

		ThreadFiber = new fiber;
		ThreadFiber->base.threadId = -1;
		ThreadFiber->base.data = nullptr;

		getcontext(&ThreadFiber->ctx);

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

		getcontext(&f->ctx);
		f->ctx.uc_stack.ss_sp = new char[FIBER_STACK_SIZE];
		f->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
		f->ctx.uc_link = 0;
		makecontext(&f->ctx, (void(*)())&FiberMain, 1, f);
		
		swapcontext(&CurrentFiber->ctx, &f->ctx);

		return f;
	}

	void FiberDestroy(fiber * f)
	{
		BASIS_ASSERT(CurrentFiber != f);
		delete [] (char *)f->ctx.uc_stack.ss_sp;
		delete f;
	}

	void FiberInvoke(fiber * f)
	{
		BASIS_ASSERT(f != nullptr);
		BASIS_ASSERT(f != CurrentFiber);
		fiber * self = CurrentFiber;
		
		if (_setjmp(self->jmp) == 0)
		{
			_longjmp(f->jmp, 1);
		}
		else
		{
			BASIS_ASSERT_FAILED;
		}

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
