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

#include <basis/signal.h>

namespace taco
{
	void ProfilerEmit(profiler_object object, profiler_action action);
}

/*
	// Thread view
	[thread 0
		[fiber A
			task
			task
			task
				switch - out]
		[fiber B
			task
			task
			task
				switch - out]
		[fiber A
				switch - in
			task
			task
				switch - out]
		[fiber C
			task
				switch - out]
		[fiber B
				switch - in
			task
			task
			...]
		...]
	
	// fiber view
	[fiber A
		task
		task
		task
			switch - out
			sleeping
			switch - in
			working
			switch - out
			sleeping
			switch - in
		task
		task
		...]

	// task view
	[task "foobar"
		<enter>
		doSomething()
		doSomething()
		wait on condition
		sleeping
		condition triggered
		doSomething()
			switch - out
			sleeping
			switch - in
		doSomething()
		<complete>]
*/
