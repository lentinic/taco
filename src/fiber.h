/*
Chris Lentini
http://divergentcoder.io

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

namespace taco
{
	struct fiber;

	typedef std::function<void()> fiber_fn;

	struct fiber_base
	{
		fiber_fn    		fn;
		fiber_fn    		onEnter;
		fiber_fn    		onExit;
		int         		threadId;
		void *      		data;
		const char *		name;
		bool        		isBlocking;
	};

	void    FiberInitializeThread();
	void    FiberShutdownThread();
	fiber * FiberCreate(const fiber_fn & fn);
	void    FiberDestroy(fiber * f);
	void    FiberInvoke(fiber * f);
	fiber * FiberCurrent();
	fiber * FiberPrevious();
	fiber * FiberRoot();
}
