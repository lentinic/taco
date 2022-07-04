/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <ucontext.h>
#include <setjmp.h>

#include <functional>
#include <atomic>

#include <basis/assert.h>
#include <basis/thread_util.h>

#include "../fiber.h"
#include "../config.h"

#if defined(__clang__)
// yes, yes, ucontext is deprecated on at least MacOS as of 10.6
// probably elsewhere too
// it still works for now though
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

// using technique detailed at http://www.1024cores.net/home/lock-free-algorithms/tricks/fibers

namespace taco
{
    struct fiber
    {
        fiber_base                base;
        ucontext_t                ctx;
        jmp_buf                   jmp;
        char *                    stack;
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

    static void FiberMain(uint32_t ptr_hi, uint32_t ptr_low)
    {
        // makecontext expects integer parameters
        // Are those 32-bit or 64-bit integers? Depends...
        // e.g. making this take a void * works find on x86_64 Linux
        // but on an arm64 mac the pointer is truncated (i.e. we get the lower 32-bits)
        // so we break the pointer up explicitly into two 32-bit integers
        // and then reconstruct the 64-bit pointer value
        // kind of ugly, but should work fairly broadly... I hope
        
        static_assert(sizeof(fiber*) <= sizeof(uint64_t));

        fiber * self = (fiber *)(((uint64_t)ptr_hi << 32) | (uint64_t)ptr_low);
        BASIS_ASSERT(self != nullptr);

        if (_setjmp(self->jmp) == 0)
        {
            swapcontext(&self->ctx, &CurrentFiber->ctx);
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
        f->base.isBlocking = false;
        f->base.onEnter = f->base.onExit = nullptr;

        f->stack = new char[FIBER_STACK_SIZE];

        getcontext(&f->ctx);
        f->ctx.uc_stack.ss_sp = f->stack;
        f->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
        f->ctx.uc_link = 0;

        uintptr_t offs_f = (uintptr_t) f;
        makecontext(&f->ctx, (void(*)())&FiberMain, 2, ((offs_f >> 32) & 0xffffffff), (offs_f & 0xffffffff));

        swapcontext(&CurrentFiber->ctx, &f->ctx);
        return f;
    }

    void FiberDestroy(fiber * f)
    {
        BASIS_ASSERT(CurrentFiber != f);
        delete [] f->stack;
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
