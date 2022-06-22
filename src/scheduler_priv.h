/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
*/

#pragma once

#include <thread>
#include <mutex>
#include <vector>
#include <condition_variable>

#include <taco/scheduler.h>

#define INVALID_SCHEDULER_ID 0xffffffff

namespace taco
{	
	struct fiber;
	
	void Suspend();
	void Suspend(std::function<void()> on_suspend);
	
	void Resume(fiber * f);
	bool IsSchedulerThread();
}
