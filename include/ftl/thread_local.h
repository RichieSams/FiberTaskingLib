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

#include "ftl/task_scheduler.h"

#include <vector>

namespace ftl {

template<class T>
class ThreadLocal;

/**
 * A handle to your thread's T. You must be careful that you don't jump to another thread in the mean time, but this will allow
 * you to prevent calls to ftl::TaskScheduler::GetCurrentThreadIndex() every single time you want to modify the object (similar to TLS optimization).
 * This is the recommended way of doing that over getting a reference as it allows instrumentation and debugging within the library.
 *
 * @tparam T    Type stored in thread local storage.
 */
template<class T>
class ThreadLocalHandle {
	friend ThreadLocal<T>;

	T& operator*() {
		return m_value;
	}
	T* operator->() {
		return &m_value;
	}
private:
	explicit ThreadLocalHandle(T& v) : m_value{v} {};

	T& m_value;
};

/**
 * Fiber compatible version of the thread_local keyword. Acts like a pointer. Think of this as you would the thread_local keyword,
 * not something you put in containers, but a specification of a variable itself. Non-copyable and non-movable.
 * For proper semantics, all variables that are ThreadLocal<> must be static.
 * 
 * @tparam T    Object type to store.
 */
template<class T>
class ThreadLocal {
private:
	template<class VP_T>
	struct alignas(64) ValuePadder {
		template<class... Args>
		explicit ValuePadder(Args&&... args) : m_value(std::forward<Args>(args)...) {}

		VP_T m_value;
	};

	static_assert(sizeof(ValuePadder<T>) % 64 == 0, "alignas() force-padding failed");
public:
	/**
	 * Default construct all T's inside the thread local variable.
	 *
	 * @param ts    The task scheduler to be thread local to.
	 */
	explicit ThreadLocal(TaskScheduler* ts) : m_scheduler{ts}, m_data{ts->GetThreadCount()} {}

	/**
	 * Emplace construct all T's inside the thread local variable using perfect forwarding.
	 *
	 * @tparam Arg1    Type of the first argument to the constructor
	 * @tparam Args    Variadic types of the other arguments to the constructor
	 * @param ts       The task scheduler to be thread local to
	 * @param arg1     First argument to the constructor
	 * @param args     Rest of the arguments to the constructor
	 */
	template<class Arg1, class... Args>
	ThreadLocal(TaskScheduler* ts, Arg1&& arg1, Args&&... args) : m_scheduler{ts}, m_data{} {
		auto const threads = m_scheduler->GetThreadCount();

		m_data.reserve(threads);

		for (std::size_t i = 0; i < threads; ++i) {
			m_data.emplace_back(std::forward<Arg1>(arg1), std::forward<Args>(args)...);
		}
	}

	ThreadLocal(ThreadLocal const& other) = delete;
	ThreadLocal(ThreadLocal&& other) noexcept = delete;
	ThreadLocal& operator=(ThreadLocal const& other) = delete;
	ThreadLocal& operator=(ThreadLocal&& other) noexcept = delete;

	/**
	 * Get a handle to your thread's T. You must be careful that you don't jump to another thread in the mean time, but this will allow
	 * you to prevent calls to ftl::TaskScheduler::GetCurrentThreadIndex() every single time you want to modify the object (similar to TLS optimization).
	 * This is the recommended way of doing that over getting a reference as it allows instrumentation and debugging within the library.
	 *
	 * @return    Handle to the thread's version of T.
	 */
	ThreadLocalHandle<T> GetHandle() {
		return ThreadLocalHandle<ValuePadder<T>>{**this};
	}

	T& operator*() {
		return m_data[m_scheduler->GetCurrentThreadIndex()];
	}
	T* operator->() {
		return &m_data[m_scheduler->GetCurrentThreadIndex()];
	}
private:
	TaskScheduler* m_scheduler;
	std::vector<T> m_data;
};

// C++17 deduction guide
#ifdef __cpp_deduction_guides
template<class T> ThreadLocal(TaskScheduler*, T) -> ThreadLocal<T>;
#endif
}
