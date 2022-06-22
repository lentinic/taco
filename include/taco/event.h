/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <deque>
#include "mutex.h"

namespace taco
{
    struct fiber;
    class event
    {
    public:
        event();
        ~event();
        
        void wait();
        void signal();
        void reset();

        operator bool () const
        {
            return m_ready.load(std::memory_order_relaxed);
        }

    private:
        event(const event &) = delete;
        event & operator = (const event & ) = delete;

        std::deque<fiber *>        m_waiting;
        mutex                      m_mutex;
        std::atomic<bool>          m_ready;
    };
}
