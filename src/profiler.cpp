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
#include <basis/shared_mutex.h>
#include <taco/profiler.h>
#include "profiler_priv.h"

namespace taco
{
	std::mutex ProfilerMutex;
	basis::signal<void(profiler_event)> ProfilerSignal;

	void ProfilerEmit(profiler_object object, profiler_action action)
	{
#if defined(TACO_PROFILER_ENABLED)
		ProfilerMutex.shared_lock();
		ProfilerSignal(object,action,nullptr,basis::GetTimestamp());
		ProfilerMutex.shared_unlock();
#endif
	}

	basis::handle32 InstallProfiler(profiler_function fn)
	{
		std::exclusive_lock<std::mutex> lock(ProfilerMutex);
		return ProfilerSignal.connect(fn);
	}

	void UninstallProfiler(basis::handle32 id)
	{
		std::exclusive_lock<std::mutex> lock(ProfilerMutex);
		ProfilerSignal.disconnect(id);
	}

#if defined(TACO_PROFILER_ENABLED)
	ProfilerScope::ProfilerScope(const char * name)
		:	m_scopeName(name)
	{
		ProfilerMutex.shared_lock();
		ProfilerSignal(profiler_object::user, profiler_action::start, name, basis::GetTimestamp());
		ProfilerMutex.shared_unlock();
	}

	ProfilerScope::~ProfilerScope()
	{
		ProfilerMutex.shared_lock();
		ProfilerSignal(profiler_object::user, profiler_action::complete, m_scopeName, basis::GetTimestamp());
		ProfilerMutex.shared_unlock();
	}
#else
	ProfilerScope::ProfilerScope(const char * name) {}
	ProfilerScope::~ProfilerScope() {}
#endif
}
