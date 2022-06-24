/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
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
        fiber_base        base;
        void *            handle;
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
