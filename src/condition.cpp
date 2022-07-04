/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <mutex>
#include <basis/assert.h>
#include <taco/condition.h>
#include <taco/taco_core.h>
#include "fiber.h"
#include "scheduler_priv.h"
#include "profiler_priv.h"

namespace taco
{
    condition::condition()
    {}

    condition::~condition()
    {
        while (!m_waiting.empty())
        {
            fiber * f = m_waiting.front();
            FiberDestroy(f);
            m_waiting.pop_front();
        }
    }

    void condition::_wait(std::function<void()> on_suspend)
    {
        TACO_PROFILER_LOG("condition::wait <%p>", this);

        fiber * cur = FiberCurrent();
        BASIS_ASSERT(cur);

        Suspend([&]() -> void {
            std::unique_lock<mutex> lock(m_mutex);
            m_waiting.push_back(cur);
            on_suspend();
        });
    }

    void condition::notify_one()
    {
        TACO_PROFILER_LOG("condition::notify_one <%p>", this);

        std::unique_lock<mutex> lock(m_mutex);
        fiber * f = m_waiting.front();
        m_waiting.pop_front();
        Resume(f);
    }

    void condition::notify_all()
    {
        TACO_PROFILER_LOG("condition::notify_all <%p>", this);
        
        std::unique_lock<mutex> lock(m_mutex);
        while (!m_waiting.empty())
        {
            fiber * f = m_waiting.front();
            m_waiting.pop_front();
            Resume(f);
        }
    }
}
