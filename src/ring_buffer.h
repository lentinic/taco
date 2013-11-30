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
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <atomic>
#include <mutex>

#include "cache_line.h"
#include "async_access_policy.h"

namespace taco
{
	namespace internal
	{
		template<class TYPE>
		struct ring_slot
		{
			TYPE				m_data;
			std::atomic_bool	m_used;
			PADDING(CACHE_LINE_SZ - sizeof(TYPE) - sizeof(std::atomic_bool));
		};
	}

	template<class TYPE, size_t LENGTH, async_access_policy POLICY = async_access_policy::spsc>
	class ring_buffer
	{
	public:
		ring_buffer()
		{
			m_head = 0;
			m_tail = 0;
			m_count = 0;

			for (size_t i=0; i<LENGTH; i++)
				m_items[i].m_used.store(false);
		}

		bool push_back(const TYPE & data)
		{
			size_t tail = m_tail;
			auto s = &m_items[tail];
			if (s->m_used.load(std::memory_order_acquire))
				return false;

			s->m_data = data;
			s->m_used.store(true, std::memory_order_release);
			m_tail = ((tail + 1) == LENGTH) ? 0 : (tail + 1);
			m_count.fetch_add(1, std::memory_order_relaxed);
			return true;
		}

		bool pop_front(TYPE * dest)
		{
			size_t head = m_head;
			auto s = &m_items[head];
			if (!s->m_used.load(std::memory_order_acquire))
				return false;

			*dest = s->m_data;
			s->m_used.store(false, std::memory_order_release);
			m_head = ((head + 1) == LENGTH) ? 0 : (head + 1);
			m_count.fetch_add(-1, std::memory_order_relaxed);
			return true;
		}

		size_t count() const
		{
			return m_count.load(std::memory_order_relaxed);
		}

		bool empty() const
		{
			const auto s = &m_items[m_head];
			if (!s->m_used.load(std::memory_order_acquire))
				return true;

			return false;
		}

	private:
		internal::ring_slot<TYPE>	m_items[LENGTH];
		size_t						m_head;
		CACHE_LINE();
		size_t						m_tail;
		CACHE_LINE();
		std::atomic<size_t>	 		m_count;
	};

	template<class TYPE, size_t LENGTH>
	class ring_buffer<TYPE, LENGTH, async_access_policy::spmc>
	{
	public:
		ring_buffer()
		{
			m_head = 0;
			m_tail = 0;
			m_count = 0;

			for (size_t i=0; i<LENGTH; i++)
				m_items[i].m_used.store(false);
		}

		bool push_back(const TYPE & data)
		{
			size_t tail = m_tail;
			auto s = &m_items[tail];
			if (s->m_used.load(std::memory_order_acquire))
				return false;

			s->m_data = data;
			s->m_used.store(true, std::memory_order_release);
			m_tail = ((tail + 1) == LENGTH) ? 0 : (tail + 1);
			m_count.fetch_add(1, std::memory_order_relaxed);
			return true;
		}

		bool pop_front(TYPE * dest)
		{
			size_t head = m_head.load(std::memory_order_relaxed);

			for (;;)
			{
				auto s = &m_items[head];
				if (!s->m_used.load(std::memory_order_acquire))
				{
					if (!m_head.compare_exchange_weak(head, head, std::memory_order_relaxed))
						continue;

					return false;
				}

				size_t next = (head + 1) == LENGTH ? 0 : (head + 1);
				if (m_head.compare_exchange_weak(head, next, std::memory_order_relaxed))
				{
					*dest = s->m_data;
					s->m_used.store(false, std::memory_order_release);
					break;
				}
			}
			m_count.fetch_add(-1, std::memory_order_relaxed);
			return true;
		}

		size_t count() const
		{
			return m_count.load(std::memory_order_relaxed);
		}

		bool empty() const
		{
			size_t head = m_head.load(std::memory_order_relaxed);
			const auto s = &m_items[head];
			if (!s->m_used.load(std::memory_order_acquire))
				return true;
			
			return false;
		}

	private:
		internal::ring_slot<TYPE>	m_items[LENGTH];
		std::atomic_size_t			m_head;
		CACHE_LINE();
		size_t						m_tail;
		CACHE_LINE();
		std::atomic_size_t			m_count;
	};

	template<class TYPE, size_t LENGTH>
	class ring_buffer<TYPE, LENGTH, async_access_policy::mpsc>
	{
	public:
		ring_buffer()
		{
			m_head = 0;
			m_tail = 0;
			m_count = 0;

			for (size_t i=0; i<LENGTH; i++)
				m_items[i].m_used.store(false);
		}

		bool push_back(const TYPE & data)
		{
			size_t tail = m_tail.load(std::memory_order_relaxed);

			for (;;)
			{
				auto s = &m_items[tail];
				if (s->m_used.load(std::memory_order_acquire))
				{
					if (!m_tail.compare_exchange_weak(tail, tail, std::memory_order_relaxed))
						continue;

					return false;
				}

				size_t next = (tail + 1) == LENGTH ? 0 : (tail + 1);
				if (m_tail.compare_exchange_weak(tail, next, std::memory_order_relaxed))
				{
					s->m_data = data;
					s->m_used.store(true, std::memory_order_release);
					break;
				}
			}
			m_count.fetch_add(1, std::memory_order_relaxed);
			return true;
		}

		bool pop_front(TYPE * dest)
		{
			size_t head = m_head;
			auto s = &m_items[head];
			if (!s->m_used.load(std::memory_order_acquire))
				return false;

			*dest = s->m_data;
			s->m_used.store(false, std::memory_order_release);
			m_head = ((head + 1) == LENGTH) ? 0 : (head + 1);
			m_count.fetch_add(-1, std::memory_order_relaxed);
			return true;
		}

		size_t count() const
		{
			return m_count.load(std::memory_order_relaxed);
		}

		bool empty() const
		{
			const auto s = &m_items[m_head];
			if (!s->m_used.load(std::memory_order_acquire))
				return true;

			return false;
		}

	private:
		internal::ring_slot<TYPE>	m_items[LENGTH];
		size_t						m_head;
		CACHE_LINE();
		std::atomic_size_t			m_tail;
		CACHE_LINE();
		std::atomic_size_t			m_count;
	};

	template<class TYPE, size_t LENGTH>
	class ring_buffer<TYPE, LENGTH, async_access_policy::mpmc>
	{
	public:
		ring_buffer()
		{
			m_head = 0;
			m_tail = 0;
			m_count = 0;

			for (size_t i=0; i<LENGTH; i++)
				m_items[i].m_used.store(false);
		}

		bool push_back(const TYPE & data)
		{
			size_t tail = m_tail.load(std::memory_order_relaxed);

			for (;;)
			{
				auto s = &m_items[tail];
				if (s->m_used.load(std::memory_order_acquire))
				{
					if (!m_tail.compare_exchange_weak(tail, tail, std::memory_order_relaxed))
						continue;

					return false;
				}

				size_t next = (tail + 1) == LENGTH ? 0 : (tail + 1);
				if (m_tail.compare_exchange_weak(tail, next, std::memory_order_relaxed))
				{
					s->m_data = data;
					s->m_used.store(true, std::memory_order_release);
					break;
				}
			}

			m_count.fetch_add(1, std::memory_order_relaxed);
			return true;
		}

		bool pop_front(TYPE * dest)
		{
			size_t head = m_head.load(std::memory_order_relaxed);

			for (;;)
			{
				auto s = &m_items[head];
				if (!s->m_used.load(std::memory_order_acquire))
				{
					if (!m_head.compare_exchange_weak(head, head, std::memory_order_relaxed))
						continue;

					return false;
				}

				size_t next = (head + 1) == LENGTH ? 0 : (head + 1);
				if (m_head.compare_exchange_weak(head, next, std::memory_order_relaxed))
				{
					*dest = s->m_data;
					s->m_used.store(false, std::memory_order_release);
					break;
				}
			}

			m_count.fetch_add(-1, std::memory_order_relaxed);
			return true;
		}

		size_t count() const
		{
			return m_count.load(std::memory_order_relaxed);
		}

		bool empty() const
		{
			size_t head = m_head.load(std::memory_order_relaxed);
			const auto s = &m_items[head];
			if (!s->m_used.load(std::memory_order_acquire))
				return true;

			return false;
		}

	private:
		internal::ring_slot<TYPE>	m_items[LENGTH];
		std::atomic_size_t			m_head;
		CACHE_LINE();
		std::atomic_size_t			m_tail;
		CACHE_LINE();
		std::atomic_size_t			m_count;
	};
}
