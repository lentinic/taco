/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <ucontext.h>
#include <setjmp.h>

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
        bool                      active;
        ucontext_t                ctx;
        jmp_buf                   jmp;
        char *                    stack;
    };

    // thread local state to let us track fiber transitions
    // not publicly constructible/copyable/moveable
    // an instance can only be accessed via the static
    // "get" method
    // this method is non-inlineable to prevent compiler
    // optimizations from caching the result across
    // fiber transitions (where a fiber can suspend and
    // then potentially resumes on a different thread)
    namespace {
        class thread_state
        {
        public:
            fiber * current {};
            fiber * previous {};
            fiber * root {};

            __attribute__((noinline))
            static thread_state & get()
            {
                basis_thread_local static thread_state state;
                return state;
            }

        private:
            thread_state() {}
            thread_state(const thread_state &) = delete;
            thread_state & operator = (const thread_state &) = delete;
        };
    }

    static void FiberHandoff(fiber * prev, fiber * cur)
    {
        thread_state & state = thread_state::get();

        BASIS_ASSERT(prev->active);
        BASIS_ASSERT(!cur->active);
        
        prev->active = false;
        cur->active = true;
        
        state.previous = prev;
        state.current = cur;

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
    }

    static void FiberMain(uint32_t ptr_hi, uint32_t ptr_low)
    {
        // makecontext expects integer parameters
        // Are those 32-bit or 64-bit integers? Depends...
        // e.g. making this take a void * seems to work fine on x86_64 Linux
        // but on an arm64 mac the pointer is truncated (i.e. we get the lower 32-bits)
        // so we break the pointer up explicitly into two 32-bit integers
        // and then reconstruct the 64-bit pointer value
        // kind of ugly, but should work fairly broadly... I hope
        
        static_assert(sizeof(fiber*) <= sizeof(uint64_t));

        fiber * self = (fiber *)(((uint64_t)ptr_hi << 32) | (uint64_t)ptr_low);

        BASIS_ASSERT(self != nullptr);

        // save the jump point, we will resume from here the first
        // time the fiber actually gets invoked (and the condition
        // will fail, dropping us below)
        if (_setjmp(self->jmp) == 0)
        {
            // swap back to the fiber we were in when this one was created
            fiber * origin = thread_state::get().current;
            swapcontext(&self->ctx, &origin->ctx);
        }
        
        // First time this fiber has been invoked, complete the handoff
        // transition from whatever fiber jumped to us before invoking
        // the users function
        FiberHandoff(thread_state::get().current, self);
        self->base.fn();
    }

    void FiberInitializeThread()
    {
        thread_state & state = thread_state::get();

        BASIS_ASSERT(state.root == nullptr);

        fiber * root = new fiber;
        root->base.threadId = -1;
        root->base.data = nullptr;
        root->active = true;

        state.root = state.current = root;
    }

    void FiberShutdownThread()
    {  
        thread_state & state = thread_state::get();

        BASIS_ASSERT(state.current == state.root);

        delete state.root;
        state.root = state.current = nullptr;
    }

    fiber * FiberCreate(const fiber_fn & fn)
    {
        fiber * f = new fiber;
        uintptr_t addr = (uintptr_t) f;

        f->base.fn = fn;
        f->base.threadId = -1;
        f->base.data = nullptr;
        f->base.isBlocking = false;
        f->base.onEnter = f->base.onExit = nullptr;
        f->active = false;
        f->stack = new char[FIBER_STACK_SIZE];

        getcontext(&f->ctx);

        f->ctx.uc_stack.ss_sp = f->stack;
        f->ctx.uc_stack.ss_size = FIBER_STACK_SIZE;
        f->ctx.uc_link = 0;

        makecontext(&f->ctx, (void(*)())&FiberMain, 2, ((addr >> 32) & 0xffffffff), (addr & 0xffffffff));
        swapcontext(&thread_state::get().current->ctx, &f->ctx);

        return f;
    }

    void FiberDestroy(fiber * f)
    {
        BASIS_ASSERT(thread_state::get().current != f);

        delete [] f->stack;
        delete f;
    }

    void FiberInvoke(fiber * f)
    {
        fiber * self = thread_state::get().current;

        BASIS_ASSERT(f != self);

        if (_setjmp(self->jmp) == 0)
        {
            _longjmp(f->jmp, 1);
        }

        FiberHandoff(thread_state::get().current, self);
    }

    fiber * FiberCurrent()
    {
        return thread_state::get().current;
    }

    fiber * FiberPrevious()
    {
        return thread_state::get().previous;
    }

    fiber * FiberRoot()
    {
        return thread_state::get().root;
    }
}
