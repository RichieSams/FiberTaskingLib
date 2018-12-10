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

#ifndef FTL_THREAD_LOCAL_HANDLE_DEBUG
#	define FTL_THREAD_LOCAL_HANDLE_DEBUG FTL_DEBUG
#endif

#include "ftl/task_scheduler.h"

#include <functional>

namespace ftl {

template <class T>
class ThreadLocal;

/**
 * A handle to your thread's T. You must be careful that you don't jump to another thread in the mean time, but this will allow
 * you to prevent calls to ftl::TaskScheduler::GetCurrentThreadIndex() every single time you want to modify the object (similar to TLS
 * optimization). This is the recommended way of doing that over getting a reference as it allows instrumentation and debugging within the
 * library.
 *
 * @tparam T    Type stored in thread local storage.
 */
template <class T>
class ThreadLocalHandle {
private:
	friend ThreadLocal<T>;

	void ValidHandle(T &value);

public:
	T &operator*() {
		ValidHandle(m_value);
		return m_value;
	}
	T *operator->() {
		ValidHandle(m_value);
		return &m_value;
	}

private:
	ThreadLocalHandle(ThreadLocal<T> &parent, T &v) : m_parent{parent}, m_value{v} {
	}

	ThreadLocal<T> &m_parent;
	T &m_value;
};

/**
 * Fiber compatible version of the thread_local keyword. Acts like a pointer. Think of this as you would the thread_local keyword,
 * not something you put in containers, but a specification of a variable itself. Non-copyable and non-movable.
 * For proper semantics, all variables that are ThreadLocal<> must be static.
 *
 * @tparam T    Object type to store.
 */
template <class T>
class ThreadLocal {
private:
	template <class VpT>
	struct alignas(kCacheLineSize) ValuePadder {
		ValuePadder() : Value() {
		}
		VpT Value;
		bool Initialized = true;
	};

public:
	/**
	 * Default construct all T's inside the thread local variable.
	 *
	 * @param ts    The task scheduler to be thread local to.
	 */

#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(                                                                                                                       \
	    disable : 4316) // I know this won't be allocated to the right alignment, this is okay as we're using alignment for padding.
#endif                  // _MSC_VER
	explicit ThreadLocal(TaskScheduler *ts) : m_scheduler{ts}, m_initializer{}, m_data{new ValuePadder<T>[ts->GetThreadCount()]} {
	}
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER

	/**
	 * Construct all T's by calling a void factory function the first time you use your data.
	 *
	 * @tparam F         Type of the factory function
	 * @param ts         The task scheduler to be thread local to
	 * @param factory    Factory function to initialize the values with.
	 */
	template <class F>
	ThreadLocal(TaskScheduler *ts, F &&factory)
	        : m_scheduler{ts},
	          m_initializer{std::forward<F>(factory)},
	          m_data(static_cast<ValuePadder<T> *>(operator new[](sizeof(ValuePadder<T>) * ts->GetThreadCount()))) {
		for (std::size_t i = 0; i < ts->GetThreadCount(); ++i) {
			// That's not how placement new works...
			// ReSharper disable once CppNonReclaimedResourceAcquisition
			new (&m_data[i].Initialized) bool(false);
		}
	}

	ThreadLocal(ThreadLocal const &other) = delete;
	ThreadLocal(ThreadLocal &&other) noexcept = delete;
	ThreadLocal &operator=(ThreadLocal const &other) = delete;
	ThreadLocal &operator=(ThreadLocal &&other) noexcept = delete;

	~ThreadLocal() {
		delete[] m_data;
	}

	/**
	 * Get a handle to your thread's T. You must be careful that you don't jump to another thread in the mean time, but this will allow
	 * you to prevent calls to ftl::TaskScheduler::GetCurrentThreadIndex() every single time you want to modify the object (similar to TLS
	 * optimization). This is the recommended way of doing that over getting a reference as it allows instrumentation and debugging within
	 * the library.
	 *
	 * @return    Handle to the thread's version of T.
	 */
	ThreadLocalHandle<T> GetHandle() {
		return ThreadLocalHandle<T>{*this, **this};
	}

	T &operator*() {
		std::size_t idx = m_scheduler->GetCurrentThreadIndex();
		InitValue(idx);
		return m_data[idx].Value;
	}
	T *operator->() {
		std::size_t idx = m_scheduler->GetCurrentThreadIndex();
		InitValue(idx);
		return &m_data[idx].Value;
	}

	/**
	 * Returns a copy of all the elements within the thread_local value. Operation is _not_ thread safe,
	 * it does not take any locks or copy any object atomically.
	 *
	 * @return    Vector of all the values.
	 */
	std::vector<T> GetAllValues() {
		std::vector<T> vec;
		std::size_t const threads = m_scheduler->GetThreadCount();
		vec.reserve(threads);

		for (std::size_t i = 0; i < threads; ++i) {
			InitValue(i);
			vec.emplace_back(m_data[i].Value);
		}

		return vec;
	}

	/**
	 * Returns a reference to all of the elements within the thread_local value. Operation is thread safe within the
	 * the thread_local object itself, but does not guarantee thread safety when accessing the references.
	 *
	 * @return    Vector of all the values.
	 */
	std::vector<std::reference_wrapper<T>> GetAllValuesByRef() {
		std::vector<T> vec;
		std::size_t const threads = m_scheduler->GetThreadCount();
		vec.reserve(threads);

		for (std::size_t i = 0; i < threads; ++i) {
			InitValue(i);
			vec.emplace_back(std::ref(m_data[i].Value));
		}

		return vec;
	}

private:
	void InitValue(std::size_t idx) {
		// I add this exception with careful consideration due to the scale of the warning. There is no way to get here
		// without running into a operator new on this member, or on the parent object, setting the value.
		// I _think_ it is complaining because I might not call operator new on the parent as I get the object
		// from a generic operator new instance. There is a way to do this which avoids the problem but it isn't
		// as space efficient.
		// NOLINTNEXTLINE(clang-analyzer-core.uninitialized.Branch)
		if (!m_data[idx].Initialized) {
			// That's not how placement new works...
			// ReSharper disable once CppNonReclaimedResourceAcquisition
			new (&m_data[idx].Value) T(m_initializer());
			m_data[idx].Initialized = true;
		}
	}

	TaskScheduler *m_scheduler;
	std::function<T()> m_initializer;
	ValuePadder<T> *m_data;
};

#if FTL_THREAD_LOCAL_HANDLE_DEBUG
template <class T>
void ThreadLocalHandle<T>::ValidHandle(T &value) {
	if (&*m_parent != &value) {
		FTL_ASSERT("Invalid ThreadLocalHandle", false);
	}
}
#else
template <class T>
void ThreadLocalHandle<T>::ValidHandle(T &value) {
}
#endif

// C++17 deduction guide
#ifdef __cpp_deduction_guides
template <class T>
ThreadLocal(TaskScheduler *, T)->ThreadLocal<T>;
#endif
} // namespace ftl
