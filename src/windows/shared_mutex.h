/*
Copyright (c) 2013 Chris Lentini
http://divergentcoder.com

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
#include <windows.h>

namespace taco
{
	class shared_mutex
	{
	public:
		shared_mutex()
		{}

		bool try_lock_shared()
		{
			return TryAcquireSRWLockShared(&m_rwlock);
		}

		void lock_shared()
		{
			AcquireSRWLockShared(&m_rwlock);
		}

		void unlock_shared()
		{
			ReleaseSRWLockShared(&m_rwlock);
		}

		bool try_lock()
		{
			return TryAcquireSRWLockExclusive(&m_rwlock);
		}

		void lock(const unsigned spincount = 50)
		{
			AcquireSRWLockExclusive(&m_rwlock);
		}

		void unlock()
		{
			ReleaseSRWLockExclusive(&m_rwlock);
		}

	private:
		shared_mutex(const shared_mutex &);
		shared_mutex & operator = (const shared_mutex &);

		SRWLOCK m_rwlock = SRWLOCK_INIT;
	};
}
