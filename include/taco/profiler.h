/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <basis/handle.h>
#include <basis/timer.h>
#include <basis/string.h>

namespace taco
{
    namespace profiler
    {
        enum class event_type
        {
            schedule,
            start,
            suspend,
            resume,
            complete,
            sleep,
            awake,
            log,
            enter_scope,
            exit_scope
        };

        struct event
        {
            basis::tick_t   timestamp;
            uint64_t        task_id;
            uint64_t        thread_id;
            event_type      type;
            const char *    message;
        };

        typedef void(listener_fn)(event);

        listener_fn * InstallListener(listener_fn * fn);

        void Log(const char * fmt, ...);

        class scope
        {
        public:
            scope(const char * name);
            ~scope();

        private:
            scope(const scope &) = delete;
            scope & operator = (const scope &) = delete;

            const char * m_name;
        };
    }
}

