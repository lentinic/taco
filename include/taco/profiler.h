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
		enum class event_object
		{
			none,
			thread,
			fiber,
			task,
			scope
		};

		enum class event_action
		{
			schedule,
			start,
			suspend,
			resume,
			complete
		};

		struct event
		{
			event_object	object;
			event_action	action;
			basis::tick_t	timestamp;
			const char *	name;
		};

		typedef std::function<void(event)> listener_fn;

		basis::handle32 InstallListener(listener_fn fn);
		void UninstallListener(basis::handle32 id);

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
