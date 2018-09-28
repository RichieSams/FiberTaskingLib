/**
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2018
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "../atomic_counter.h"
#include <type_traits>
#include <tuple>
#include <memory>


namespace ftl {
	struct TrampolineBase {
		TrampolineBase(void* arg = nullptr) : arg(arg) {}
		void* arg;

		virtual ~TrampolineBase() = default;
	};

	template<class F, class... Args> struct BoundTrampoline;

	template<class F>
	struct Trampoline : TrampolineBase {
		Trampoline(F&& f, void* userdata = nullptr) : TrampolineBase(userdata), handler(std::move(f)) {}
		F handler;

		template<class... Args>
		auto bind(Args&&... args) {
			return std::unique_ptr<BoundTrampoline<F, Args...>>(new BoundTrampoline(*this, std::forward_as_tuple(args...)));
		}

		virtual ~Trampoline() = default;
	};

	namespace detail {
		template<class T>
		struct return_value_t {
			typename T::type return_value;
			operator typename T::type() { return return_value; }

		};

		template<class F, class... Args>
		using return_type_of_lambda = typename std::invoke_result<F, Args...>;

		template <typename T>
		struct has_type {
		private:
			template <typename T1>
			static typename T1::type test(int);
			template <typename>
			static void test(...);
		public:
			enum { value = !std::is_void<decltype(test<T>(0))>::value };
		};

		template<class F, class... Args>
		constexpr bool is_lambda_void_return = !has_type<return_type_of_lambda<F, Args...>>::value;

		struct empty {};
	}

	template<class F, class... Args>
	struct BoundTrampoline : BoundTrampolineBase, std::conditional_t < detail::is_lambda_void_return<F, Args...>, detail::empty, detail::return_value_t<detail::return_type_of_lambda<F, Args...>> > {
		Trampoline<F> tramp;
		std::tuple<Args...> args;
		static constexpr bool is_void_return = detail::is_lambda_void_return<F, Args...>;

		BoundTrampoline(Trampoline<F> func, std::tuple<Args...>&& args) : tramp(func), args(std::move(args)) {}

		// call fiber
		auto call() {
			return std::apply(tramp.handler, args);
		}

		// call task
		static void gencall(ftl::TaskScheduler* ts, void * arg) {
			auto& t = *static_cast<BoundTrampoline<F, Args...>*>(arg);
			if constexpr (!is_void_return)
				t.return_value = t.call();
			else
				t.call();
		}

		virtual operator ftl::Task() & override {
			return ftl::Task{ &gencall, static_cast<void*>(this) };
		}

		virtual ~BoundTrampoline() = default;
	};

} // End of namespace ftl
