/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <utility>

#include <basis/assert.h>
#include <basis/thread_util.h>
#include <basis/chunk_queue.h>
#include <basis/string.h>
#include <basis/shared_mutex.h>

#include "scheduler_priv.h"
#include "config.h"
#include "fiber.h"
#include "profiler_priv.h"
#include "thread_state.h"

#include "work_queue.h"

namespace taco
{
    struct task_entry
    {
        task_fn         fn;
        basis::string   name;
        uint64_t        id;

        void operator () ()
        {
            thread_state<task_entry *>() = this;
            TACO_PROFILER_EMIT(profiler::event_type::start, name);
            fn();
            TACO_PROFILER_EMIT(profiler::event_type::complete);
        }
    };

    typedef work_queue<task_entry, 256> shared_task_queue_t;
    typedef work_queue<fiber*, 256> shared_fiber_queue_t;
    typedef basis::chunk_queue<task_entry,basis::queue_access_policy::mpsc> private_task_queue_t;
    typedef basis::chunk_queue<fiber *,basis::queue_access_policy::mpsc> private_fiber_queue_t;

    struct scheduler_data
    {
        scheduler_data()
        :   privateTasks(PRIVATE_TASKQ_CHUNK_SIZE),
            privateFibers(PRIVATE_FIBERQ_CHUNK_SIZE)
        {}

        shared_task_queue_t         sharedTasks;
        private_task_queue_t        privateTasks;
        shared_fiber_queue_t        sharedFibers;
        private_fiber_queue_t       privateFibers;

        std::thread                 thread;

        std::atomic_bool            exitRequested;
        std::condition_variable_any wakeCondition;
        basis::shared_mutex         wakeMutex;
        std::vector<fiber*>         inactive;
        std::atomic<uint32_t>       privateTaskCount;

        uint32_t                    threadId;
        bool                        isActive;
        bool                        isSignaled;
    };

    static std::atomic<uint32_t> GlobalSharedTaskCount;
    static scheduler_data * SchedulerList = nullptr;
    static uint32_t ThreadCount = 0;
    static std::atomic<uint64_t> GlobalTaskCounter;

    struct blocking_thread
    {
        std::thread                 thread;
        std::mutex                  workMutex;
        std::condition_variable     workCondition;
        bool                        exitRequested;
        fiber *                     current;
    };

    basis::ring_queue<blocking_thread*,basis::queue_access_policy::mpmc> BlockingThreads(BLOCKING_THREAD_LIMIT);
    std::atomic<int> BlockingThreadCount(0);

    static void WorkerLoop();

    uint64_t GenTaskId()
    {
        return GlobalTaskCounter.fetch_add(1);
    }

    static bool HasTasks()
    {
        uint32_t shared_count = GlobalSharedTaskCount.load(std::memory_order_relaxed);
        uint32_t private_count = thread_state<scheduler_data*>()->privateTaskCount.load(std::memory_order_relaxed);

        return (shared_count > 0) || (private_count > 0);
    }

    static bool GetPrivateTask(task_entry & out)
    {
        return thread_state<scheduler_data*>()->privateTasks.pop_front(out);
    }

    static bool GetSharedTask(task_entry & out)
    {
        uint32_t start = thread_state<scheduler_data*>()->threadId;
        uint32_t id = start;
        do
        {
            if ((id == start) && SchedulerList[id].sharedTasks.pop(out))
            {
                GlobalSharedTaskCount.fetch_sub(1, std::memory_order_relaxed);
                return true;
            } 
            else if ((id != start) && SchedulerList[id].sharedTasks.steal(out))
            {
                GlobalSharedTaskCount.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
            id = (id + 1) < ThreadCount ? (id + 1) : 0;
        } while (id != start && GlobalSharedTaskCount > 0);
        return false;
    }

    static void SignalScheduler(scheduler_data * s)
    {
        {
            s->wakeMutex.lock_shared();
            s->isSignaled = true;
            s->wakeMutex.unlock_shared();
        }

        s->wakeCondition.notify_one();
    }

    static void AskForHelp(size_t count)
    {
        TACO_PROFILER_LOG("Help Requested");

        uint32_t start = thread_state<scheduler_data*>()->threadId;
        uint32_t id = (start + 1) < ThreadCount ? (start + 1) : 0;
        while (count > 0 && id != start)
        {
            if (SchedulerList[id].isActive)
            {
                SignalScheduler(SchedulerList + id);
                count--;
            }
            id = (id + 1) < ThreadCount ? (id + 1) : 0;
        }
    }

    static fiber * GetInactiveFiber()
    {
        size_t count = thread_state<scheduler_data*>()->inactive.size();
        if (count > 0)
        {
            fiber * f = thread_state<scheduler_data*>()->inactive[count - 1];
            thread_state<scheduler_data*>()->inactive.pop_back();
            return f;
        }
        return FiberCreate(&WorkerLoop);
    }

    static fiber * GetSharedFiber()
    {
        fiber * ret = nullptr;
        uint32_t start = thread_state<scheduler_data*>()->threadId;
        uint32_t id = start;
        do
        {
            if ((id == start) && SchedulerList[id].sharedFibers.pop(ret))
            {
                return ret;
            }
            else if ((id != start) && SchedulerList[id].sharedFibers.steal(ret))
            {
                return ret;
            }
            id = (id + 1) < ThreadCount ? (id + 1) : 0;
        } while (id != start);
        return nullptr;
    }

    static fiber * GetNextScheduledFiber()
    {
        fiber * ret = nullptr;

        // TODO:  Something more intelligent...
        basis_thread_local static int source = 0;

        if (source == 0)
        {
            if (!thread_state<scheduler_data*>()->privateFibers.pop_front(ret))
            {
                ret = GetSharedFiber();
            }
        }
        else
        {
            ret = GetSharedFiber();
            if (!ret)
            {
                thread_state<scheduler_data*>()->privateFibers.pop_front(ret);
            }
        }
        source = (ret != nullptr) ? (source ^ 1) : source;

        return ret;
    }

    static fiber * GetNextFiber()
    {
        fiber * next = HasTasks() ? GetInactiveFiber() : GetNextScheduledFiber();
        next = (next == nullptr) ? GetInactiveFiber() : next;
        return next;
    }

    static void CheckForExitCondition()
    {
        if (thread_state<scheduler_data*>()->exitRequested)
        {
            thread_state<scheduler_data*>()->inactive.push_back(FiberCurrent());
            FiberInvoke(FiberRoot());
            BASIS_ASSERT_FAILED;
        }
    }

    static void FiberSwitch(fiber * to)
    {
        BASIS_ASSERT(!((fiber_base *)to)->isBlocking);
        TACO_PROFILER_EMIT(profiler::event_type::suspend);
        
        task_entry * task = thread_state<task_entry*>();
        FiberInvoke(to);
        thread_state<task_entry*>() = task;

        CheckForExitCondition();

        TACO_PROFILER_EMIT(profiler::event_type::resume);
    }

    static bool WorkerIteration()
    {
        fiber * self = FiberCurrent();
        fiber_base * base = (fiber_base *) self;

        task_entry todo;
        if (GetPrivateTask(todo))
        {
            thread_state<scheduler_data*>()->privateTaskCount.fetch_sub(1, std::memory_order_relaxed);
            base->threadId = thread_state<scheduler_data*>()->threadId;
            base->data = nullptr;
            base->name = todo.name;
            todo();
            base->name = "";
            basis::strfree(todo.name);

            return true;
        }
        else if (GetSharedTask(todo))
        {
            base->threadId = -int(thread_state<scheduler_data*>()->threadId + 1);
            base->data = nullptr;
            base->name = todo.name;
            todo();
            base->name = "";
            basis::strfree(todo.name);

            return true;
        }
        else
        {
            fiber * next = GetNextScheduledFiber();
            if (next)
            {
                thread_state<scheduler_data*>()->inactive.push_back(self);
                FiberSwitch(next);
                return true;
            }
        }
        return false;
    }

    static void WorkerLoop()
    {
        for(;;)
        {
            CheckForExitCondition();

            if (!WorkerIteration())
            {
                std::unique_lock<basis::shared_mutex> lock(thread_state<scheduler_data*>()->wakeMutex);
                if (!thread_state<scheduler_data*>()->isSignaled)
                {
                    TACO_PROFILER_EMIT(profiler::event_type::sleep)
                    thread_state<scheduler_data*>()->wakeCondition.wait(lock);
                    TACO_PROFILER_EMIT(profiler::event_type::awake)
                }
                thread_state<scheduler_data*>()->isSignaled = false;
            }
        }
    }

    static void ShutdownScheduler()
    {
        fiber * f = nullptr;
        BASIS_ASSERT(FiberCurrent() == FiberRoot());
        while (thread_state<scheduler_data*>()->privateFibers.pop_front(f))
        {
            FiberDestroy(f);
        }

        while (thread_state<scheduler_data*>()->sharedFibers.pop(f))
        {
            FiberDestroy(f);
        }

        for (size_t i=0; i<thread_state<scheduler_data*>()->inactive.size(); i++)
        {
            FiberDestroy(thread_state<scheduler_data*>()->inactive[i]);
        }
        thread_state<scheduler_data*>()->inactive.clear();

        thread_state<scheduler_data*>() = nullptr;
    }

    void Initialize(int nthreads)
    {
        BASIS_ASSERT(ThreadCount == 0);

        ThreadCount = (nthreads <= 0) ? std::thread::hardware_concurrency() : nthreads;
        GlobalSharedTaskCount = 0;

        SchedulerList = new scheduler_data[ThreadCount];
        for (unsigned i=0; i<ThreadCount; i++)
        {
            SchedulerList[i].exitRequested = false;
            SchedulerList[i].threadId = i;
            SchedulerList[i].isActive = false;
            SchedulerList[i].isSignaled = false;
        }

        for (unsigned i=1; i<ThreadCount; i++)
        {
            SchedulerList[i].thread = std::thread([=]() -> void {
                auto & scheduler = thread_state<scheduler_data*>();
                scheduler = SchedulerList + i;

                FiberInitializeThread();
                fiber * f = FiberCreate(&WorkerLoop);
                
                thread_state<scheduler_data*>()->isActive = true;
                FiberInvoke(f);
                thread_state<scheduler_data*>()->isActive = false;

                ShutdownScheduler();
                FiberShutdownThread();
            });
        }

        thread_state<scheduler_data*>() = SchedulerList;
        FiberInitializeThread();
    }

    void Initialize(const char * name, task_fn comain, int nthreads)
    {
        Initialize(nthreads);
        Schedule(name, [&]() -> void {
            comain();
            ExitMain();
        }, 0);
        EnterMain();
    }

    void Shutdown()
    {
        auto & scheduler = thread_state<scheduler_data*>();
        // Must be called from main thread and main thread must not
        // be in the scheduler loop
        BASIS_ASSERT(scheduler == SchedulerList && !scheduler->isActive);

        for (unsigned i=1; i<ThreadCount; i++)
        {
            SchedulerList[i].exitRequested = true;
            SignalScheduler(SchedulerList + i);
        }

        for (unsigned i=1; i<ThreadCount; i++)
        {
            SchedulerList[i].thread.join();
        }

        blocking_thread * thread;
        while (BlockingThreads.pop_front(thread))
        {
            {
                std::unique_lock<std::mutex> lock(thread->workMutex);
                thread->exitRequested = true;
            }
            thread->workCondition.notify_one();
            thread->thread.join();
            delete thread;
        }
        
        ShutdownScheduler();

        delete [] SchedulerList;
        SchedulerList = nullptr;
        ThreadCount = 0;
        BlockingThreadCount = 0;

        FiberShutdownThread();
    }

    void EnterMain()
    {
        auto & scheduler = thread_state<scheduler_data*>();
        BASIS_ASSERT(scheduler == SchedulerList);
        BASIS_ASSERT(!thread_state<scheduler_data*>()->isActive);

        fiber * f = FiberCreate(&WorkerLoop);

        thread_state<scheduler_data*>()->isActive = true;
        FiberInvoke(f);
        thread_state<scheduler_data*>()->isActive = false;

        thread_state<scheduler_data*>()->exitRequested = false;
    }

    void ExitMain()
    {
        SchedulerList[0].exitRequested = true;
        SignalScheduler(SchedulerList);
    }

    void Schedule(const char * name, task_fn fn, uint32_t threadid)
    {
        BASIS_ASSERT(thread_state<scheduler_data*>() != nullptr);
        
        uint64_t taskid = GenTaskId();

        TACO_PROFILER_EMIT(profiler::event_type::schedule, taskid, name);

        if (threadid < ThreadCount)
        {
            scheduler_data * s = SchedulerList + threadid;
            s->privateTasks.push_back<task_entry>({ fn, basis::stralloc(name), taskid });
            s->privateTaskCount.fetch_add(1, std::memory_order_relaxed);

            if (s != thread_state<scheduler_data*>())
            {
                SignalScheduler(s);
            }
        }
        else
        {
            BASIS_ASSERT(threadid == constants::invalid_thread_id);
            basis::string taskname = basis::stralloc(name);
            while (!thread_state<scheduler_data*>()->sharedTasks.push({ fn, taskname, taskid })) {
                printf("Can't push task %s\n", taskname);
                Switch();
            }
            
            uint32_t count = GlobalSharedTaskCount.fetch_add(1, std::memory_order_relaxed) + 1;
            if (count > 1 || !thread_state<scheduler_data*>()->isActive)
            {
                AskForHelp(count);
            }
        }
    }

    void Switch()    
    {
        fiber_base * f = (fiber_base *) FiberCurrent();
        f->onExit = [&]() -> void {
            if (f->threadId < 0)
            {
                thread_state<scheduler_data*>()->sharedFibers.push((fiber *)f);
            }
            else
            {
                thread_state<scheduler_data*>()->privateFibers.push_back((fiber *)f);
            }
        };
        FiberSwitch(GetNextFiber());
    }

    void BlockingThread(blocking_thread * self)
    {
        FiberInitializeThread();
        std::unique_lock<std::mutex> lock(self->workMutex);
        for (;;)
        {
            while (!self->current && !self->exitRequested)
            {
                self->workCondition.wait(lock);
            }

            if (self->exitRequested)
            {
                break;
            }

            BASIS_ASSERT(self->current);

            FiberInvoke(self->current);
            self->current = nullptr;
            BlockingThreads.push_back(self);
        }
        FiberShutdownThread();
    }

    void BeginBlocking()
    {
        fiber * f = FiberCurrent();
        fiber_base * base = (fiber_base *) f;

        BASIS_ASSERT(!base->onExit);

        blocking_thread * thread = nullptr;
        while (!thread && !BlockingThreads.pop_front(thread))
        {
            int cur = BlockingThreadCount.fetch_add(1);
            if (cur == BLOCKING_THREAD_LIMIT)
            {
                BlockingThreadCount.store(cur);
                Switch();
            }
            else
            {
                thread = new blocking_thread;
                thread->exitRequested = false;
                thread->current = nullptr;
                thread->thread = std::thread([=]() -> void {
                    BlockingThread(thread);
                });
            }
        }

        base->onExit = [=]() -> void {
            base->isBlocking = true;
            {
                std::unique_lock<std::mutex> lock(thread->workMutex);
                thread->current = f;
            }
            thread->workCondition.notify_one();
        };

        FiberInvoke(GetNextFiber());
    }

    void EndBlocking()
    {
        fiber * f = FiberCurrent();
        fiber_base * base = (fiber_base *) f;
        
        BASIS_ASSERT(!base->onExit);

        base->onExit = [=]() -> void {
            base->isBlocking = false;
            if (base->threadId < 0)
            {
                SchedulerList[-(base->threadId + 1)].sharedFibers.push(f);
                SignalScheduler(SchedulerList - (base->threadId + 1));
            }
            else
            {
                BASIS_ASSERT((unsigned)base->threadId < thread_state<scheduler_data*>()->threadId);
                thread_state<scheduler_data*>()->privateFibers.push_back(f);
                SignalScheduler(thread_state<scheduler_data*>());
            }
        };

        FiberInvoke(FiberRoot());
    }

    void SetTaskLocalData(void * data)
    {
        fiber_base * f = (fiber_base *) FiberCurrent();
        f->data = data;
    }

    void * GetTaskLocalData()
    {
        fiber_base * f = (fiber_base *) FiberCurrent();
        return f->data;
    }

    const char * GetTaskName()
    {
        fiber_base  * f = (fiber_base *) FiberCurrent();
        return f->name;
    }

    void Suspend()
    {
        FiberSwitch(GetNextFiber());
    }

    void Suspend(std::function<void()> on_suspend)
    {
        fiber_base * f = (fiber_base *) FiberCurrent();
        BASIS_ASSERT(!f->onExit);
        f->onExit = on_suspend;
        FiberSwitch(GetNextFiber());
    }
    
    void Resume(fiber * f)
    {
        fiber_base * base = (fiber_base *) f;
        if (base->threadId < 0)
        {
            thread_state<scheduler_data*>()->sharedFibers.push(f);
        }
        else
        {
            BASIS_ASSERT(base->threadId >= 0 && (unsigned)base->threadId < ThreadCount);
            SchedulerList[base->threadId].privateFibers.push_back(f);
            SignalScheduler(SchedulerList + base->threadId);
        }
    }

    bool IsSchedulerThread()
    {
        return thread_state<scheduler_data*>() != nullptr && thread_state<scheduler_data*>()->isActive;
    }

    uint32_t GetSchedulerId()
    {
        return thread_state<scheduler_data*>() ? thread_state<scheduler_data*>()->threadId : INVALID_SCHEDULER_ID;
    }

    uint64_t GetTaskId()
    {
        return thread_state<task_entry*>()->id;
    }

    uint32_t GetThreadCount()
    {
        return ThreadCount;    
    }
}
