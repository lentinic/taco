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

#pragma once

#include <functional>
#include "fiber_status.h"

namespace taco
{
	typedef struct fiber_data * fiber_id;
	typedef std::function<void()> fiber_fn;

	class fiber
	{
	public:
		fiber();
		explicit fiber(const fiber_fn & fn);
		explicit fiber(fiber_id fid);

		fiber(const fiber & other);
		fiber(fiber && other);
		fiber & operator = (const fiber & other);
		fiber & operator = (fiber && other);
		~fiber();

		static void initialize_thread();
		static void shutdown_thread();
		static void	yield_to(const fiber & other);
		static void yield();
		static void	run(fiber_id id);
		static fiber current();

		fiber_status status() const;

		fiber_id id() const { return m_id; }
		void operator () () const { fiber::run(m_id); }
		operator bool () const { return m_id != nullptr; }

	private:
		fiber_id m_id;
	};
}
