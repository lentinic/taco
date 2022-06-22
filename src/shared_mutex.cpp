/*
Copyright (c) 2014 Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <basis/assert.h>
#include <basis/thread_util.h>
#include <taco/scheduler.h>
#include <taco/shared_mutex.h>
#include "scheduler_priv.h"
#include "profiler_priv.h"
#include "config.h"

#define EXCLUSIVE 0x80000000

namespace taco
{
    shared_mutex::shared_mutex()
        : m_state(0)
    {}

    bool shared_mutex::try_lock_shared()
    {
        BASIS_ASSERT(IsSchedulerThread());
        
        TACO_PROFILER_LOG("shared_mutex::try_lock_shared <%p>", this);

        uint32_t state = m_state.fetch_add(1);
        if (!!(state & EXCLUSIVE))
        {
            state = m_state.fetch_sub(1);
            return false;
        }
        return true;
    }
    
    void shared_mutex::lock_shared()
    {
        BASIS_ASSERT(IsSchedulerThread());

        TACO_PROFILER_LOG("shared_mutex::lock_shared <%p>", this);
        unsigned counter = 0;
        while (!try_lock_shared())
        {
            counter++;
            if (counter == MUTEX_SPIN_COUNT)
            {
                Switch();
                counter = 0;
            }
            else
            {
                basis::cpu_yield();
            }
        }
    }

    void shared_mutex::unlock_shared()
    {
        BASIS_ASSERT(IsSchedulerThread());

        TACO_PROFILER_LOG("shared_mutex::unlock_shared <%p>", this);
        uint32_t state = m_state.fetch_sub(1);

        BASIS_ASSERT((state & (~EXCLUSIVE)) != 0);
    }

    bool shared_mutex::try_lock()
    {
        BASIS_ASSERT(IsSchedulerThread());

        TACO_PROFILER_LOG("shared_mutex::try_lock <%p>", this);
        uint32_t expected = 0;
        return m_state.compare_exchange_strong(expected, EXCLUSIVE);
    }

    bool shared_mutex::try_lock_weak()
    {
        BASIS_ASSERT(IsSchedulerThread());

        TACO_PROFILER_LOG("shared_mutex::try_lock <%p>", this);
        uint32_t expected = 0;
        return m_state.compare_exchange_weak(expected, EXCLUSIVE);    
    }

    void shared_mutex::lock()
    {
        BASIS_ASSERT(IsSchedulerThread());

        TACO_PROFILER_LOG("shared_mutex::lock <%p>", this);

        // Try to acquire the lock, but if we can't try and prevent further shared locks
        uint32_t state = m_state.fetch_or(EXCLUSIVE);
        unsigned counter = 0;

        while (state != 0)
        {
            if ((state & EXCLUSIVE) == 0)
            {
                // we own the next lock, wait for shared locks to release
                while ((m_state.load() & (~EXCLUSIVE)) != 0)
                {
                    counter++;
                    if (counter == MUTEX_SPIN_COUNT)
                    {
                        Switch();
                        counter = 0;
                    }
                    else
                    {
                        basis::cpu_yield();
                    }
                }    
                return;
            }

            // someone else owns the lock or is waiting on the lock

            counter++;
            if (counter == MUTEX_SPIN_COUNT)
            {
                Switch();
                counter = 0;
            }
            else
            {
                basis::cpu_yield();
            }

            state = m_state.fetch_or(EXCLUSIVE);
        }
    }

    void shared_mutex::unlock()
    {
        BASIS_ASSERT(IsSchedulerThread());

        TACO_PROFILER_LOG("shared_mutex::unlock <%p>", this);
        uint32_t state = m_state.fetch_and(~EXCLUSIVE);
        BASIS_ASSERT(!!(state & EXCLUSIVE));
    }
}
