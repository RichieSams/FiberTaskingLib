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

#include "ftl/task.h"

#include <type_traits>
#include <tuple>
#include <functional>

namespace ftl {
	class TaskScheduler;

	template<class F, class... Args> struct BoundTrampoline;

	template<class F>
	struct Trampoline {
		Trampoline(F&& f) : handler(std::move(f)) {}
		F handler;

		template<class... Args>
		BoundTrampoline<F, Args...>* bind(Args&&... args) {
			return new BoundTrampoline<F, Args...>(*this, std::forward_as_tuple(args...));
		}
	};

	template<class F>
	Trampoline<F> make_trampoline(F&& f) {
		Trampoline<F> t(std::forward<F>(f));
		return t;
	}

	/*
		C++11 machinery to power std::apply
	*/
	namespace detail {
		// based on http://stackoverflow.com/a/17426611/410767 by Xeo
		template <size_t... Ints>
		struct index_sequence {
			using type = index_sequence;
			using value_type = size_t;
			static constexpr std::size_t size() noexcept { return sizeof...(Ints); }
		};

		// --------------------------------------------------------------

		template <class Sequence1, class Sequence2>
		struct _merge_and_renumber;

		template <size_t... I1, size_t... I2>
		struct _merge_and_renumber<index_sequence<I1...>, index_sequence<I2...>>
			: index_sequence<I1..., (sizeof...(I1) + I2)...> {};

		// --------------------------------------------------------------

		template <size_t N>
		struct make_index_sequence
			: _merge_and_renumber<typename make_index_sequence<N / 2>::type,
			typename make_index_sequence<N - N / 2>::type> {};

		template<> struct make_index_sequence<0> : index_sequence<> {};
		template<> struct make_index_sequence<1> : index_sequence<0> {};


		template<typename F, typename... Args>
		auto invoke(F f, Args&&... args) -> decltype(std::ref(f)(std::forward<Args>(args)...)) {
			return std::ref(f)(std::forward<Args>(args)...);
		}

		template <class F, class Tuple, std::size_t... I>
		auto apply_impl(F&& f, Tuple&& t, detail::index_sequence<I...>) -> decltype(std::ref(f)(std::get<I>(std::forward<Tuple>(t))...)) {
			return invoke(std::forward<F>(f), std::get<I>(std::forward<Tuple>(t))...);
		}

	}  // namespace detail

	template<class F, class... Args>
	struct BoundTrampoline {
		Trampoline<F> tramp;
		std::tuple<Args...> args;

		friend struct Trampoline<F>;
	private:
		BoundTrampoline(Trampoline<F> func, std::tuple<Args...>&& args) : tramp(func), args(std::move(args)) {}
	public:
		BoundTrampoline(const BoundTrampoline&) = delete;
		BoundTrampoline& operator=(const BoundTrampoline&) = delete;

		BoundTrampoline(BoundTrampoline&&) = default;
		BoundTrampoline& operator=(BoundTrampoline&&) = default;

		// call task
		static void gencall(ftl::TaskScheduler* ts, void * arg) {
			auto t = static_cast<BoundTrampoline<F, Args...>*>(arg);
			detail::apply_impl(t->tramp.handler, t->args,detail::make_index_sequence<std::tuple_size<typename std::remove_reference<decltype(args)>::type>::value>{});
			delete t;
		}

		operator Task() {
			return Task{ &gencall, static_cast<void*>(this) };
		}

		~BoundTrampoline() = default;
	};

} // End of namespace ftl
