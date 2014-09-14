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

#pragma once

#include <functional>
#include <stdint.h>

#define TACO_INVALID_THREAD_ID 0xffffffff

namespace taco
{
	typedef std::function<void()> task_fn;

	void Initialize(int nthreads = -1);
	void Initialize(const char * name, task_fn comain, int nthreads = -1);

	inline void Initialize(task_fn comain, int nthreads = -1)
	{
		Initialize("CoMain", comain, nthreads);
	}

	void Shutdown();
	void EnterMain();
	void ExitMain();

	void Schedule(const char * name, task_fn fn, uint32_t threadid = TACO_INVALID_THREAD_ID);

	inline void Schedule(task_fn fn, uint32_t threadid = TACO_INVALID_THREAD_ID)
	{
		Schedule(nullptr, fn, threadid);
	}

	void SetTaskLocalData(void * data);
	void * GetTaskLocalData();
	const char * GetTaskName();
	void Switch();
	
	void BeginBlocking();
	void EndBlocking();

	uint32_t GetThreadCount();
	uint32_t GetSchedulerId();
}
