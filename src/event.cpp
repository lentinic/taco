/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <mutex>
#include <basis/assert.h>
#include <taco/event.h>
#include <taco/scheduler.h>
#include "fiber.h"
#include "scheduler_priv.h"
#include "profiler_priv.h"

namespace taco
{
    event::event()
    {}

    event::~event()
    {
        while (!m_waiting.empty())
        {
            fiber * f = m_waiting.front();
            FiberDestroy(f);
            m_waiting.pop_front();
        }
    }

    void event::wait()
    {
        BASIS_ASSERT(IsSchedulerThread());
        TACO_PROFILER_LOG("event::wait <%p>", this);

        fiber * cur = FiberCurrent();
        BASIS_ASSERT(cur);

        if (m_ready.load(std::memory_order_relaxed))
        {
            return;
        }

        m_mutex.lock();
        if (m_ready.load(std::memory_order_relaxed))
        {
            m_mutex.unlock();
            return;
        }

        Suspend([&]() -> void {
            m_waiting.push_back(cur);
            m_mutex.unlock();
        });
    }

    void event::signal()
    {
        BASIS_ASSERT(IsSchedulerThread());
        TACO_PROFILER_LOG("event::signal <%p>", this);
        
        m_mutex.lock();
        m_ready = true;
        m_mutex.unlock();
        
        while (!m_waiting.empty())
        {
            auto f = m_waiting.front();
            Resume(f);
            m_waiting.pop_front();
        }
    }

    void event::reset()
    {
        BASIS_ASSERT(m_waiting.empty());
        m_ready = false;
    }
}
