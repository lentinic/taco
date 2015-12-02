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

#include <basis/handle.h>
#include <basis/timer.h>

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

		void DebugLog(const char * file, int line, const char * fmt, ...);
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

#if defined(TACO_PROFILER_ENABLED)
#define TACO_PROFILER_SCOPE(name) taco::profiler::scope BASIS_CONCAT(tmp_scope, __LINE__)(name)
#else
#define TACO_PROFILER_SCOPE(name)
#endif

#if defined(TACO_PROFILE_ENABLED)
#define TACO_PROFILER_DEBUG_LOG(fmt, ...) taco::profiler::DebugLog(__FILE__, __LINE__, fmt, __VA_ARGS__)
#define TACO_PROFILER_LOG(fmt, ...) taco::profiler::DebugLog(fmt, __VA_ARGS__)
#else
#define TACO_PROFILER_DEBUG_LOG(fmt, ...)
#define TACO_PROFILER_LOG(fmt, ...)
#endif
