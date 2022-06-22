/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <utility>
#include <type_traits>
#include <taco/event.h>

namespace taco
{
    namespace internal
    {
        template<class TYPE>
        struct generator_state
        {
            event         evtWrite;
            event         evtRead;
            TYPE          data;
            bool          complete;
        };
    };

    template<class TYPE>
    struct generator
    {
        bool read(TYPE & dest) const
        {
            BASIS_ASSERT(state);
            if (state->complete)
            {
                dest = std::move(state->data);
                return false;
            }

            state->evtWrite.wait();
            state->evtWrite.reset();

            dest = std::move(state->data);
            state->evtRead.signal();

            return true;
        }

        bool completed() const
        {
            return state->complete;
        }

        operator TYPE () const
        {
            BASIS_ASSERT(state);

            TYPE rval;
            read(rval);
            return rval;
        }

        std::shared_ptr<internal::generator_state<TYPE>> state;
    };

    template<class TYPE>
    void YieldValue(TYPE && data)
    {
        typedef typename std::remove_reference<TYPE>::type return_type;

        generator<return_type> * current = (generator<return_type> *)GetTaskLocalData();

        current->state->data = std::forward<TYPE>(data);
        current->state->evtWrite.signal();

        current->state->evtRead.wait();
        current->state->evtRead.reset();
    }

    template<class F>
    auto StartGenerator(const char * name, F fn, uint32_t threadid = TACO_INVALID_THREAD_ID) -> generator<decltype(fn())>
    {
        typedef decltype(fn()) return_type;

        generator<return_type> r = { std::make_shared<internal::generator_state<return_type>>() };
        r.state->complete = false;

        Schedule(name, [=]() mutable {
            SetTaskLocalData(&r);
            r.state->data = fn();
            r.state->complete = true;
            r.state->evtWrite.signal();
        }, threadid);

        return r;
    }

    template<class F>
    auto StartGenerator(F fn, uint32_t threadid = TACO_INVALID_THREAD_ID) -> generator<decltype(fn())>
    {
        return StartGenerator(nullptr, fn, threadid);
    }
}
