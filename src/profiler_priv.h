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
		void Emit(event_object object, event_action action, const char * name);
	}
}

#if defined(TACO_PROFILER_ENABLED)
#define TACO_PROFILER_EMIT_FULL(object, action, name) taco::profiler::Emit(object, action, name)
#define TACO_PROFILER_EMIT_NONAME(object, action) taco::profiler::Emit(object, action, "")
#define TACO_PROFILER_EMIT_ACTION(action) taco::profiler::Emit(taco::profiler::event_object::none, action, "")
#else
#define TACO_PROFILER_EMIT_FULL(object, action, name)
#define TACO_PROFILER_EMIT_NONAME(object, action)
#define TACO_PROFILER_EMIT_ACTION(action)
#endif

#define TACO_PROFILER_SELECTOR(tuple) TACO_PROFILER_SELECTOR_IMPL tuple
#define TACO_PROFILER_SELECTOR_IMPL(_1,_2,_3,N,...) N
#define TACO_PROFILER_EMIT(...) \
	TACO_PROFILER_SELECTOR((__VA_ARGS__,TACO_PROFILER_EMIT_FULL,TACO_PROFILER_EMIT_NONAME,TACO_PROFILER_EMIT_ACTION))(__VA_ARGS__)
