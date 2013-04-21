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

namespace taco
{
	enum class assert_response
	{
		Break,
		Continue
	};

	typedef assert_response (*assert_handler)(const char *, const char *, int, const char *);

	assert_handler GetAssertHandler();
	assert_handler SetAssertHandler(assert_handler handler);

	assert_response AssertFailed(const char * cnd, const char * file, int line, const char * msg, ...);
}

#if defined(_MSC_VER)
#define TACO_DEBUG_BREAK __debugbreak()
#else
#error "Assert support undefined for current platform"
#endif

#if defined(_DEBUG) || defined(TACO_ASSERTS_ENABLED)

extern "C" const int TacoAssertDummy;

#define TACO_ASSERT(cnd) \
	do \
	{ \
		if (!(cnd) && taco::AssertFailed(#cnd, __FILE__, __LINE__, NULL) == taco::assert_response::Break) \
		{ \
			TACO_DEBUG_BREAK; \
		} \
	} while(TacoAssertDummy != 0)

#define TACO_ASSERT_MSG(cnd, msg, ...) \
	do \
	{ \
		if (!(cnd) && taco::AssertFailed(#cnd, __FILE__, __LINE__, msg, __VA_ARGS__) == taco::assert_response::Break) \
		{ \
			TACO_DEBUG_BREAK; \
		} \
	} while(TacoAssertDummy != 0)

#define TACO_ASSERT_FAILED \
	do \
	{ \
		if (taco::AssertFailed("No Condition", __FILE__, __LINE__, NULL) == taco::assert_response::Break) \
		{ \
			TACO_DEBUG_BREAK; \
		} \
	} while(TacoAssertDummy != 0)

#define TACO_ASSERT_FAILED_MSG(msg, ...) \
	do \
	{ \
		if (taco::AssertFailed("No Condition", __FILE__, __LINE__, msg, __VA_ARGS__) == taco::assert_response::Break) \
		{ \
			TACO_DEBUG_BREAK; \
		} \
	} while(TacoAssertDummy != 0)

#else
#define TACO_ASSERT(cnd)
#define TACO_ASSERT_MSG(cnd, msg, ...)
#define TACO_ASSERT_FAILED
#define TACO_ASSERT_FAILED_MSG(msg, ...)
#endif
