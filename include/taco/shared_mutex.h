/*
Copyright (c) 2014 Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <atomic>

namespace taco
{
	class shared_mutex
	{
	public:
		shared_mutex();

		bool try_lock_shared();
		void lock_shared();
		void unlock_shared();

		bool try_lock();
		bool try_lock_weak();
		void lock();
		void unlock();

	private:
		shared_mutex(const shared_mutex &);
		shared_mutex & operator = (const shared_mutex &);

		std::atomic<uint32_t> m_state;
	};
}
