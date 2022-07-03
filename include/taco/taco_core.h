/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <functional>
#include <stdint.h>

namespace taco
{
    typedef std::function<void()> task_fn;

    namespace constants {
        static constexpr uint32_t invalid_thread_id = 0xffffffff;
    }

    void Initialize(int nthreads = -1);
    void Initialize(const char * name, task_fn comain, int nthreads = -1);

    void Shutdown();
    void EnterMain();
    void ExitMain();

    void Schedule(const char * name, task_fn fn, uint32_t threadid = constants::invalid_thread_id);

    void SetTaskLocalData(void * data);
    void * GetTaskLocalData();
    const char * GetTaskName();
    void Switch();
    
    void BeginBlocking();
    void EndBlocking();

    uint32_t GetThreadCount();
    uint32_t GetSchedulerId();
    uint64_t GetTaskId();

    inline void Initialize(task_fn comain, int nthreads = -1)
    {
        Initialize("CoMain", comain, nthreads);
    }
    
    inline void Schedule(task_fn fn, uint32_t threadid = constants::invalid_thread_id)
    {
        Schedule(nullptr, fn, threadid);
    }

}
