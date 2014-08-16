/*
Copyright (c) 2013 Chris Lentini
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
#include "profiler_priv.h"

namespace taco
{
	namespace profiler
	{
		basis::shared_mutex SignalMutex;
		basis::signal<void(event)> EventSignal;

		void Emit(event_object object, event_action action, const char * name)
		{
			SignalMutex.lock_shared();
			EventSignal({ object, action, basis::GetTimestamp(), name });
			SignalMutex.unlock_shared();
		}

		basis::handle32 InstallListener(listener_fn fn)
		{
			std::unique_lock<basis::shared_mutex> lock(SignalMutex);
			return EventSignal.connect(fn);
		}

		void UninstallListener(basis::handle32 id)
		{
			std::unique_lock<basis::shared_mutex> lock(SignalMutex);
			EventSignal.disconnect(id);
		}

		scope::scope(const char * name)
			: 	m_name(name)
		{
			Emit(event_object::scope, event_action::start, name);
		}

		scope::~scope()
		{
			Emit(event_object::scope, event_action::complete, m_name);
		}

	}
}
