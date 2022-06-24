/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <basis/assert.h>

namespace taco
{
    template<class MTYPE>
    class shared_lock
    {
    public:
        shared_lock(MTYPE & m)
            :    m_mutex(&m)
        {
            m_mutex->lock_shared();
            m_locked = true;
        }

        shared_lock(shared_lock && other)
            :    m_mutex(other.m_mutex),
                m_locked(other.m_locked)
        {
            other.m_mutex = nullptr;
            other.m_locked = false;
        }

        shared_lock & operator = (shared_lock && other)
        {
            if (m_mutex && m_locked)
            {
                m_mutex->unlock_shared();
            }

            m_mutex = other.m_mutex;
            m_locked = other.m_locked;
            other.m_mutex = nullptr;
            other.m_locked = false;

            return *this;
        }

        ~shared_lock()
        {
            if (m_locked)
            {
                BASIS_ASSERT(m_mutex != nullptr);
                m_mutex->unlock_shared();
                m_locked = false;
            }
        }

        bool try_lock()
        {
            BASIS_ASSERT(m_mutex != nullptr);
            BASIS_ASSERT(!m_locked);
            m_locked = m_mutex->try_lock_shared();
            return m_locked;
        }

        void lock()
        {
            BASIS_ASSERT(m_mutex != nullptr);
            BASIS_ASSERT(!m_locked);
            m_mutex->lock_shared();
            m_locked = true;
        }

        void unlock()
        {
            BASIS_ASSERT(m_mutex != nullptr);
            BASIS_ASSERT(m_locked);
            m_mutex->unlock_shared();
            m_locked = false;
        }

    private:
        shared_lock() = delete;
        shared_lock(const shared_lock &) = delete;
        shared_lock & operator = (const shared_lock &) = delete;

        MTYPE *    m_mutex;
        bool    m_locked;
    };
}
