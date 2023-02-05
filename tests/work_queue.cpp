#include <time.h>
#include <basis/unit_test.h>
#include <taco/taco.h>
#include "../src/work_queue.h"

#define TEST_TIMEOUT_MS 2000

void test_work_queue();

BASIS_TEST_LIST_BEGIN()
    BASIS_DECLARE_TEST(test_work_queue)
BASIS_TEST_LIST_END()

void test_work_queue()
{
    srand(time(NULL));

    constexpr uint32_t num_items = 137;
    constexpr uint32_t num_threads = 8;
    constexpr uint32_t target_value = 16;

    uint32_t completed[num_threads] = {};
    uint32_t items[num_items] = {};

    std::atomic<uint32_t> total_completed = 0;
    taco::work_queue<uint32_t *, 256> queue[num_threads];

    // Push pointers to elements in items to the first queue
    for (int i=0; i<num_items; i++)
    {
        queue[0].push(items + i);
    }

    // Spin up a number of threads to work through the items in the queues
    std::thread thr[num_threads];
    for (int i=0; i<num_threads; i++)
    {
        thr[i] = std::thread([&, i]() -> void {
            uint32_t stolen = 0;
            // iterate until we've finished processing all the work items
            while (total_completed < num_items) 
            {
                uint32_t * ptr = nullptr;
                bool try_steal = !!(rand() & 1);

                // pop an item from our queue or else try and steal from
                // someone else. Go straight to stealing with 50% probability
                if (try_steal|| !queue[i].pop(ptr))
                {
                    uint32_t index = rand() % num_threads;
                    if ((index == i) || !queue[index].steal(ptr)) 
                    {
                        continue;
                    }
                    stolen++;
                }

                // Increment the value
                uint32_t val = ++(*ptr);
                if (val < target_value) 
                {
                    // If it is less than the target value we are going to
                    // flag the upper bit and sleep for a bit
                    // The expectation is that if some other thread grabs
                    // the same item because of a flaw in the queue code
                    // then this will cause the above condition to fail at
                    // some point, and then trigger a failure in the block below
                    ptr[0] |= 0x80000000;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    ptr[0] ^= 0x80000000;
                    queue[i].push(ptr);
                } 
                else
                {
                    BASIS_TEST_VERIFY_MSG(val == target_value, 
                        "Thread %u: Unexpected value %u", i, val);
                    total_completed++;
                    completed[i]++;
                }
            }
            BASIS_TEST_VERIFY_MSG(stolen != 0, 
                "Thread %u: Failed to steal anything", i);
        });
    }

    for (int i=0; i<8; i++)
    {
        thr[i].join();
    }
}

int main(int argc, char ** argv)
{
    BASIS_RUN_TESTS();
    return 0;
}
