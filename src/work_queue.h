/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <atomic>

namespace taco
{
    /// @brief Fixed size, single-producer, single-consumer (same thread) work stealing queue implementation
    /// @tparam TYPE data type stored by the queue
    /// @tparam CAPACITY maximum numner of items that can be pushed to the queue. Must be a power of 2
    template<class TYPE, uint32_t CAPACITY>
    class work_queue
    {
        static constexpr uint32_t MASK = CAPACITY - 1;   
        static_assert((CAPACITY & (CAPACITY - 1)) == 0, "Capacity must be power of 2");
        static_assert(CAPACITY < 0x80000000);

    public:
        /// @brief Adds a single item to the queue
        /// @param item
        /// @return 
        bool push(TYPE item)
        {
            uint32_t tail = m_tail.load(std::memory_order_acquire) + 1;
            uint32_t head = m_head.load(std::memory_order_acquire);

            if ((tail & MASK) != (head & MASK)) 
            {
                m_items[tail & MASK] = item;
                m_tail.store(tail, std::memory_order_release);
                return true;
            }
            
            return false;
        }

        /// @brief Removes a single item from queue in LIFO fashion
        /// @param item 
        /// @return 
        bool pop(TYPE & item)
        {
            uint32_t tail = m_tail.load(std::memory_order_acquire);
            uint32_t head = m_head.load(std::memory_order_acquire);
            
            if ((tail & MASK) != (head & MASK)) 
            {
                item = m_items[tail & MASK];

                if ((tail - 1) != head)
                {
                    m_tail.store(tail - 1, std::memory_order_release);
                    return true;
                }

                return m_head.compare_exchange_weak(head, head + 1, std::memory_order_acq_rel);
            }
            
            return false;
        }

        /// @brief Removes an item from the queue in FIFO fashion
        /// @param item 
        /// @return 
        bool steal(TYPE & item)
        {
            uint32_t head = m_head.load(std::memory_order_acquire);
            uint32_t tail = m_tail.load(std::memory_order_acquire);
            
            if ((tail & MASK) != (head & MASK)) 
            {
                item = m_items[(head + 1) & MASK];
                return m_head.compare_exchange_weak(head, head+1, std::memory_order_acq_rel);
            }

            return false;
        }

    private:
        std::atomic<uint32_t> m_head            { 0xffffffff };
        std::atomic<uint32_t> m_tail            { 0xffffffff };
        TYPE                  m_items[CAPACITY] {};
    };
}