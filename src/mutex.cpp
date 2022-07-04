/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <basis/assert.h>
#include <basis/thread_util.h>
#include <taco/taco_core.h>
#include <taco/mutex.h>
#include "scheduler_priv.h"
#include "profiler_priv.h"
#include "config.h"

namespace taco
{
    mutex::mutex()
        : m_locked(INVALID_SCHEDULER_ID)
    {}

    bool mutex::try_lock()
    {
        BASIS_ASSERT(IsSchedulerThread());
        TACO_PROFILER_LOG("mutex::try_lock <%p>", this);

        uint32_t expected = INVALID_SCHEDULER_ID;
        return m_locked.compare_exchange_strong(expected, GetSchedulerId());
    }

    bool mutex::try_lock_weak()
    {
        BASIS_ASSERT(IsSchedulerThread());
        TACO_PROFILER_LOG("mutex::try_lock_weak <%p>", this);

        uint32_t expected = INVALID_SCHEDULER_ID;
        return m_locked.compare_exchange_weak(expected, GetSchedulerId());
    }
    
    void mutex::lock()
    {
        BASIS_ASSERT(IsSchedulerThread());
        TACO_PROFILER_LOG("mutex::lock <%p>", this);

        int count = 0;
        while (!try_lock_weak())
        {
            count++;
            if (count == MUTEX_SPIN_COUNT)
            {
                Switch();
                count = 0;
            }
            else
            {
                basis::cpu_yield();
            }
        }
    }

    void mutex::unlock()
    {
        BASIS_ASSERT(IsSchedulerThread());
        TACO_PROFILER_LOG("mutex::unlock <%p>", this);

        uint32_t expected = GetSchedulerId();
        bool r = m_locked.compare_exchange_strong(expected, INVALID_SCHEDULER_ID);

        BASIS_ASSERT(r);
    }
}
