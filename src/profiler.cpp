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
#include <mutex>
#include <basis/shared_mutex.h>
#include <taco/profiler.h>
#include <taco/scheduler.h>
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

		void Emit(event_type type, const char * message)
		{
			ProfilerEventListener({  basis::GetTimestamp(), GetTaskId(), GetSchedulerId(), type, message });
		}

		listener_fn * InstallListener(listener_fn * fn)
		{
			listener_fn * prev = ProfilerEventListener;
			ProfilerEventListener = fn ? fn : &DefaultProfilerListener;
			return prev;
		}

		void DebugLog(const char * file, int line, const char * fmt, ...)
		{
		}

		void Log(const char * fmt, ...)
		{	
		}

		scope::scope(const char * name)
			: 	m_name(name)
		{
			Emit(event_type::enter_scope, name);
		}

		scope::~scope()
		{
			Emit(event_type::exit_scope, m_name);
		}

	}
}
