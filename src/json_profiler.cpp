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
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS 
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR 
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER 
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN 
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <taco/json_profiler.h>
#include <taco/scheduler.h>
#include <taco/profiler.h>
#include <basis/timer.h>
#include <basis/assert.h>
#include <basis/string.h>
#include <basis/shared_mutex.h>
#include <stdlib.h>

#include <mutex>
#include <algorithm>

#define JSON_PROFILER_VERSION 0x000000001

namespace taco
{
	struct task_entry
	{
		basis::tick_t 	start;
		basis::tick_t 	stop;
		basis::string	name;
	};

	struct fiber_entry
	{
		basis::tick_t start;
		basis::tick_t stop;
	};

	struct thread_entry
	{
		basis::tick_t 				start;
		basis::tick_t 				stop;
		std::vector<fiber_entry>	fibers;
		std::vector<task_entry>		tasks;
	};

	basis::shared_mutex ProfilerMutex;

	json_profiler::json_profiler()
	{
		listener = profiler::InstallListener([&](profiler::event evt) -> void {
			auto id = GetSchedulerId();
			if (id >= thread_data.size())
			{
				std::unique_lock<basis::shared_mutex> lock(ProfilerMutex);
				thread_data.resize(id + 1);
			}

			ProfilerMutex.lock_shared();
			switch (evt.object)
			{
			case profiler::event_object::thread:
				if (evt.action == profiler::event_action::start)
				{
					thread_data[id].start = evt.timestamp;
					thread_data[id].stop = 0;
				}
				else if (evt.action == profiler::event_action::complete)
				{
					thread_data[id].stop = evt.timestamp;
				}
				break;
				
			case profiler::event_object::fiber:
				if (evt.action == profiler::event_action::start || evt.action == profiler::event_action::resume)
				{
					thread_data[id].fibers.push_back({ evt.timestamp, 0 });
					if (evt.action == profiler::event_action::resume)
					{
						thread_data[id].tasks.push_back({ evt.timestamp, 0,  basis::stralloc(taco::GetTaskName()) });
					}	
				}
				else if (evt.action == profiler::event_action::suspend || evt.action == profiler::event_action::complete)
				{
					size_t index = thread_data[id].fibers.size() - 1;
					thread_data[id].fibers[index].stop = evt.timestamp;
					if (evt.action == profiler::event_action::suspend && thread_data[id].tasks.size() > 0)
					{
						index = thread_data[id].tasks.size() - 1;
						if (thread_data[id].tasks[index].stop == 0)
						{
							thread_data[id].tasks[index].stop = evt.timestamp;
						}
					}
				}
				break;
			case profiler::event_object::task:
				if (evt.action == profiler::event_action::start)
				{
					task_entry entry = { evt.timestamp, 0, basis::stralloc(evt.name) };
					thread_data[id].tasks.push_back(entry);
				}	
				else if (evt.action == profiler::event_action::complete)
				{
					size_t index = thread_data[id].tasks.size() - 1;
					thread_data[id].tasks[index].stop = evt.timestamp;
				}
				break;
			}
			ProfilerMutex.unlock_shared();
		});
	}

	json_profiler::~json_profiler()
	{
		profiler::UninstallListener(listener);
		for (size_t i=0; i<thread_data.size(); i++)
		{
			for (size_t j=0; j<thread_data[i].tasks.size(); j++)
			{
				basis::strfree(thread_data[i].tasks[j].name);
			}
		}
	}

	void json_profiler::write(FILE * file)
	{
		basis::tick_t start = thread_data[0].start;
		basis::tick_t end = thread_data[0].stop;
		for (size_t i=1; i<thread_data.size(); i++)
		{
			start = (start < thread_data[i].start) ? start : thread_data[i].start;
			end = (end > thread_data[i].stop) ? end : thread_data[i].stop;
		}

		// gather some stats
		std::vector<basis::tick_t> times;
		for (size_t i=0; i<thread_data.size(); i++)
		{
			times.reserve(times.size() + thread_data[i].tasks.size());
			for (size_t j=0; j<thread_data[i].tasks.size(); j++)
			{
				if (thread_data[i].tasks[j].stop == 0)
				{
					thread_data[i].tasks[j].stop = end;
				}
				times.push_back(basis::GetTimeDeltaUS(thread_data[i].tasks[j].start, thread_data[i].tasks[j].stop));
			}
		}
		std::sort(times.begin(), times.end());

		fprintf(file, "{\n\t\"version\": %d,\n\t\"elapsed\":%u,\n\t\"median\":%u,\n\t\"percentile75\":%u,\n\t\"maximum\":%u,",
			JSON_PROFILER_VERSION, 
			basis::GetTimeDeltaUS(start, end),
			times[times.size() / 2], 
			times[(times.size() * 3) / 4],
			times[times.size() - 1]);
		fprintf(file, "\n\t\"threads\":[\n");

		for (size_t i=0; i<thread_data.size(); i++)
		{
			auto t = thread_data[i];
			
			fprintf(file, "\t\t{\n");
			fprintf(file, "\t\t\t\"start\": %u,\n", t.start);
			fprintf(file, "\t\t\t\"stop\": %u,\n", t.stop);
			fprintf(file, "\t\t\t\"fibers\": [\n");

			for (size_t j=0; j<t.fibers.size(); j++)
			{
				auto ev = t.fibers[j];
				ev.stop = (ev.stop > end || ev.stop == 0) ? end : ev.stop;
				fprintf(file, "\t\t\t\t{ \"start\": %u, \"stop\": %u }%c\n",
					basis::GetTimeDeltaUS(start, ev.start),
					basis::GetTimeDeltaUS(start, ev.stop),
					(j < (t.fibers.size() - 1)) ? ',' : ' ');
			}

			fprintf(file, "\t\t\t],\n");
			fprintf(file, "\t\t\t\"tasks\": [\n");

			for (size_t j=0; j<t.tasks.size(); j++)
			{
				auto ev = t.tasks[j];
				fprintf(file, "\t\t\t\t{ \"start\": %u, \"stop\": %u, \"label\":\"%s\" }%c\n",
					basis::GetTimeDeltaUS(start, ev.start),
					basis::GetTimeDeltaUS(start, ev.stop),
					ev.name,
					(j < (t.tasks.size() - 1)) ? ',' : ' ');
			}

			fprintf(file, "\t\t\t]\n");
			fprintf(file, "\t\t}%c\n", (i < (thread_data.size() - 1)) ? ',' : ' ');
		}
		fprintf(file, "\t]\n}\n");
	}
}
