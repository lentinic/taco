/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <functional>

namespace taco
{
    struct fiber;

    typedef std::function<void()> fiber_fn;

    struct fiber_base
    {
        fiber_fn            fn;
        fiber_fn            onEnter;
        fiber_fn            onExit;
        int                 threadId;
        void *              data;
        const char *        name;
        bool                isBlocking;
    };

    void    FiberInitializeThread();
    void    FiberShutdownThread();
    fiber * FiberCreate(const fiber_fn & fn);
    void    FiberDestroy(fiber * f);
    void    FiberInvoke(fiber * f);
    fiber * FiberCurrent();
    fiber * FiberPrevious();
    fiber * FiberRoot();
}
