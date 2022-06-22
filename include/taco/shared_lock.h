/*
Copyright (c) 2014 Chris Lentini
http://divergentcoder.io

Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to use, 
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
Software, and to permit persons to whom the Software is furnished to do so, 
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANT*IES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
			:	m_mutex(&m)
		{
			m_mutex->lock_shared();
			m_locked = true;
		}

		shared_lock(shared_lock && other)
			:	m_mutex(other.m_mutex),
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

		MTYPE *	m_mutex;
		bool	m_locked;
	};
}
