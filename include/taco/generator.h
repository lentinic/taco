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

#include <utility>
#include <type_traits>
#include <taco/event.h>

namespace taco
{
	namespace internal
	{
		template<class TYPE>
		struct generator_state
		{
			event 		evtWrite;
			event 		evtRead;
			TYPE 		data;
			bool		complete;
		};
	};

	template<class TYPE>
	struct generator
	{
		bool read(TYPE & dest) const
		{
			BASIS_ASSERT(state);
			if (state->complete)
			{
				dest = std::move(state->data);
				return false;
			}

			state->evtWrite.wait();
			state->evtWrite.reset();

			dest = std::move(state->data);
			state->evtRead.signal();

			return true;
		}

		bool completed() const
		{
			return state->complete;
		}

		operator TYPE () const
		{
			BASIS_ASSERT(state);

			TYPE rval;
			read(rval);
			return rval;
		}

		std::shared_ptr<internal::generator_state<TYPE>> state;
	};

	template<class TYPE>
	void YieldValue(TYPE && data)
	{
		typedef typename std::remove_reference<TYPE>::type return_type;

		generator<return_type> * current = (generator<return_type> *)GetTaskLocalData();

		current->state->data = std::forward<TYPE>(data);
		current->state->evtWrite.signal();

		current->state->evtRead.wait();
		current->state->evtRead.reset();
	}

	template<class F>
	auto StartGenerator(F fn) -> generator<decltype(fn())>
	{
		typedef decltype(fn()) return_type;

		generator<return_type> r = { std::make_shared<internal::generator_state<return_type>>() };
		r.state->complete = false;

		Schedule([=]() mutable {
			SetTaskLocalData(&r);
			r.state->data = fn();
			r.state->complete = true;
			r.state->evtWrite.signal();
		});

		return r;
	}
}
