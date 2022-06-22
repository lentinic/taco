/*
Chris Lentini
http://divergentcoder.io

This source code is licensed under the MIT license (found in the LICENSE file in the root directory of the project)
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
			event 	ready;
			TYPE  	data;

			TYPE value() const
			{
				return data;
			}
		};

		template<>
		struct future_data<void>
		{
			event ready;

			void value() const
			{}
		};

		template<class RTYPE>
		struct future_executor
		{
			template<class F>
			future_executor(future_data<RTYPE> * r, const F & fn)
			{
				r->data = std::move(fn());
				r->ready.signal();
			}

			template<class F, class... ARG_TYPES>
			future_executor(future_data<RTYPE> * r, const F & fn, ARG_TYPES... parameters)
			{
				r->data = std::move(fn(parameters...));
				r->ready.signal();		
			}
		};

		template<>
		struct future_executor<void>
		{
			template<class F>
			future_executor(future_data<void> * r, const F & fn)
			{
				fn();
				r->ready.signal();
			}

			template<class F, class... ARG_TYPES>
			future_executor(future_data<void> * r, const F & fn, ARG_TYPES... parameters)
			{
				fn(parameters...);
				r->ready.signal();
			}
		};
	};

	template<class TYPE>
	struct future
	{	
		TYPE await() const
		{
			BASIS_ASSERT(box);
			box->ready.wait();
			return box->value();
		}

		bool ready() const
		{
			return box && box->ready;
		}

		operator TYPE () const
		{
			return await();
		}

		std::shared_ptr<internal::future_data<TYPE>> box;
	};
	
	template<class F>
	auto Start(const char * name, F fn, uint32_t threadid = TACO_INVALID_THREAD_ID) -> future<decltype(fn())>
	{
		typedef decltype(fn()) rtype;
		future<rtype> r = { std::make_shared<internal::future_data<rtype>>() };

		Schedule(name, [=]() -> void {
			internal::future_executor<rtype>(r.box.get(), fn);
		}, threadid);
		
		return r;
	}

	template<class F>
	auto Start(F fn, uint32_t threadid = TACO_INVALID_THREAD_ID) -> future<decltype(fn())>
	{
		return Start(nullptr, fn, threadid);
	}
}
