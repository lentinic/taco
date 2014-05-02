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
#include <memory>
#include <future>
#include <taco/event.h>

namespace taco
{
	namespace internal
	{
		template<class TYPE>
		struct future_data
		{
			event 		ready;
			TYPE 		data;
		};
	};

	template<class TYPE>
	struct future
	{	
		TYPE get() const
		{
			BASIS_ASSERT(box);
			box->ready.wait();
			return box->data;
		}

		bool ready() const
		{
			return box && box->ready;
		}

		operator TYPE () const
		{
			return get();
		}

		std::shared_ptr<internal::future_data<TYPE>> box;
	};

	template<class F>
	auto Start(F f) -> future<decltype(f())>
	{
		typedef decltype(f()) type;
		future<type> r = { std::make_shared<internal::future_data<type>>() };

		Schedule([=]() -> void {
			r.box->data = f();
			r.box->ready.signal();
		});
		
		return r;
	}

	template<class F, class... ARG_TYPES>
	auto Start(F f, ARG_TYPES... parameters) -> future<decltype(f(parameters...))>
	{
		typedef decltype(f(parameters...)) type;
		future<type> r = { std::make_shared<internal::future_data<type>>() };

		Schedule([=]() -> void {
			r.box->data = f(parameters...);
			r.box->ready.signal();
		});
		
		return r;	
	}
}
