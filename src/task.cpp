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

#include <taco/task.h>
#include <basis/assert.h>
#include "scheduler_priv.h"

namespace taco
{
	/*static*/ std::shared_ptr<task> task::run(const std::function<void()> & fn, int threadid)
	{
		std::shared_ptr<task> t = std::make_shared<task>();
		
		t->m_state = task_state::scheduled;

		Schedule([=]() -> void {
			t->m_state = task_state::running;
			fn();
			t->m_state = task_state::complete;
			t->m_complete.notify_all();
		}, threadid);

		return t;
	}

	void task::sync()
	{
		m_complete.wait();
	}

	task_state task::state() const
	{
		return m_state;
	}
}
