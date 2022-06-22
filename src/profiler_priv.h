/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <basis/signal.h>
#include <taco/profiler.h>

namespace taco
{
	namespace profiler
	{
		void Emit(event_type type, uint64_t taskid, const char * message);

		inline void Emit(event_type type, const char * message) 
		{
			Emit(type, GetTaskId(), message);
		}
	}
}

#if defined(TACO_PROFILE_ENABLED)
#define TACO_PROFILER_LOG(fmt, ...) taco::profiler::Log(fmt, __VA_ARGS__)
#else
#define TACO_PROFILER_LOG(fmt, ...)
#endif

#if defined(TACO_PROFILER_ENABLED)

#define TACO_PROFILER_EMIT_NAME_TASKID(type, taskid, message) \
	taco::profiler::Emit(type, taskid, message)

#define TACO_PROFILER_EMIT_NAMED(type, message) \
	taco::profiler::Emit(type, message)

#define TACO_PROFILER_EMIT_NONAME(type) \
	taco::profiler::Emit(type, "")

#else

#define TACO_PROFILER_EMIT_NAME_TASKID(type, taskid, message)
#define TACO_PROFILER_EMIT_NAME(type, message)
#define TACO_PROFILER_EMIT_NONAME(type)

#endif


#define TACO_PROFILER_SELECTOR(tuple) TACO_PROFILER_SELECTOR_IMPL tuple
#define TACO_PROFILER_SELECTOR_IMPL(_1,_2,_3,N,...) N
#define TACO_PROFILER_EMIT(...) \
	TACO_PROFILER_SELECTOR((__VA_ARGS__,TACO_PROFILER_EMIT_NAME_TASKID, \
										TACO_PROFILER_EMIT_NAME, \
										TACO_PROFILER_EMIT_NONAME))(__VA_ARGS__)
