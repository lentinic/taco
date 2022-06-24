/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <atomic>

namespace taco
{
    class mutex
    {
    public:
        mutex();

        bool try_lock();
        bool try_lock_weak();
        void lock();
        void unlock();

    private:
        mutex(const mutex &);
        mutex & operator = (const mutex & );

        std::atomic<uint32_t> m_locked;
    };
}
