/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <mutex>
#include <deque>
#include "mutex.h"

namespace taco
{
	struct fiber;
	class condition
	{
	public:
		condition();
		~condition();
		
		template<class LOCK_TYPE>
		void wait(LOCK_TYPE & lock)
		{
			_wait([&]() -> void {
				lock.unlock();
			});

			lock.lock();
		}

		void notify_one();
		void notify_all();

	private:
		condition(const condition &);
		condition & operator = (const condition & );

		void _wait(std::function<void()> on_suspend);

		std::deque<fiber *>		m_waiting;
		mutex              		m_mutex;
	};
}
