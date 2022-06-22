/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
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
	uint64_t GetTaskId();
}
