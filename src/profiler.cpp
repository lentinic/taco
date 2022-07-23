/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#include <mutex>
#include <basis/shared_mutex.h>
#include <taco/profiler.h>
#include <taco/taco_core.h>
#include "profiler_priv.h"

namespace taco
{
    namespace profiler
    {
        void DefaultProfilerListener(event evt)
        {
            BASIS_UNUSED(evt);
        }

        static listener_fn * ProfilerEventListener = &DefaultProfilerListener;

        void Emit(event_type type, uint64_t taskid, const char * message)
        {
            ProfilerEventListener({ basis::GetTimestamp(),
                                    taskid,
                                    GetSchedulerId(),
                                    type,
                                    message });
        }

        listener_fn * InstallListener(listener_fn * fn)
        {
            listener_fn * prev = ProfilerEventListener;
            ProfilerEventListener = fn ? fn : &DefaultProfilerListener;
            return prev;
        }

        void Log(const char * fmt, ...)
        {
            va_list args;
            va_start(args, fmt);
            basis::string msg = basis::strvfmt(fmt, args);
            va_end(args);

            Emit(event_type::log, msg);

            basis::strfree(msg);
        }

        scope::scope(const char * name)
            :     m_name(name)
        {
            Emit(event_type::enter_scope, name);
        }

        scope::~scope()
        {
            Emit(event_type::exit_scope, m_name);
        }

    }
}
