/*
Chris Lentini
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
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANT*IES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
