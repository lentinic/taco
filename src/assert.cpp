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

#include <taco/assert.h>
#include <stdio.h>
#include <stdarg.h>

#include "ring_buffer.h"
#include "thread_local.h"

namespace taco
{
	static thread_local assert_handler CurrentHandler = nullptr;

	#if defined(_DEBUG) || defined(TACO_ASSERTS_ENABLED)
	const int TacoAssertDummy = 0;
	#endif

	assert_response AssertFailed(const char * cnd, const char * file, int line, const char * fmt, ...)
	{
		char buf[1024];
		const char * msg = nullptr;
		if (fmt)
		{
			va_list args;
			va_start(args, fmt);
			vsnprintf(buf, sizeof(buf), fmt, args);
			va_end(args);
			msg = buf;
		}

		if (CurrentHandler != nullptr)
			return CurrentHandler(cnd, file, line, msg);

		fprintf(stderr, "Assert Failed (%s:%d): '%s'. %s\n", file, line, cnd, msg ? msg : "");
		return assert_response::Break;
	}

	assert_handler GetAssertHandler()
	{
		return CurrentHandler;
	}

	assert_handler SetAssertHandler(assert_handler handler)
	{
		assert_handler old = CurrentHandler;
		CurrentHandler = handler;
		return old;
	}
}

