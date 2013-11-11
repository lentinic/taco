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
#include <atomic>

//
// From the ticket lock implementation at http://locklessinc.com/articles/locks/
//

namespace taco
{
	class spinlock
	{
	public:
		spinlock()
			:	m_users(0), m_tickets(0)
		{}

		void lock()
		{
			uint16_t me = m_users.fetch_add(1, std::memory_order_release);
			while (m_tickets.load(std::memory_order_acquire) != me)
			{}
		}

		void unlock()
		{
			m_tickets.fetch_add(1, std::memory_order_release);
		}

	private:
		spinlock(const spinlock &);
		spinlock & operator = (const spinlock &);

		std::atomic<uint16_t> m_users;
		std::atomic<uint16_t> m_tickets;
	};
}
