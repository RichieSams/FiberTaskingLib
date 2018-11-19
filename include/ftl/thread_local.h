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
	#define FTL_THREAD_LOCAL_HANDLE_DEBUG FTL_DEBUG
#endif

#include "ftl/task_scheduler.h"

#include <functional>

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
private:
	friend ThreadLocal<T>;

	void valid_handle(T& value);

public:
	T& operator*() {
		valid_handle(m_value);
		return m_value;
	}
	T* operator->() {
		valid_handle(m_value);
		return &m_value;
	}
private:
	ThreadLocalHandle(ThreadLocal<T>& parent, T& v) : m_parent{parent}, m_value {v} {};

	ThreadLocal<T>& m_parent;
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
	struct alignas(CACHE_LINE_SIZE) ValuePadder {
		VP_T m_value;
		bool m_inited = true;
	};

public:
	/**
	 * Default construct all T's inside the thread local variable.
	 *
	 * @param ts    The task scheduler to be thread local to.
	 */
	explicit ThreadLocal(TaskScheduler* ts)
		: m_scheduler{ts},
		  m_initalizer{},
		  m_data{new ValuePadder<T>[ts->GetThreadCount()]} {}

	/**
	 * Construct all T's by calling a void factory function the first time you use your data.
	 *
	 * @tparam F      Type of the factory function
	 * @param ts      The task scheduler to be thread local to
	 * @param args    Factory function to initialize the values with.
	 */
	template<class F>
	ThreadLocal(TaskScheduler* ts, F&& factory)
	  : m_scheduler{ts},
	    m_initalizer{std::forward<F>(factory)},
	    m_data(static_cast<ValuePadder<T>*>(::operator new[](sizeof(ValuePadder<T>) * ts->GetThreadCount()))) {
        for (std::size_t i = 0; i < ts->GetThreadCount(); ++i) {
            m_data[i].m_inited = false;
        }
    }

	ThreadLocal(ThreadLocal const& other) = delete;
	ThreadLocal(ThreadLocal&& other) noexcept = delete;
	ThreadLocal& operator=(ThreadLocal const& other) = delete;
	ThreadLocal& operator=(ThreadLocal&& other) noexcept = delete;

	~ThreadLocal() {
		delete[] m_data;
	}

	/**
	 * Get a handle to your thread's T. You must be careful that you don't jump to another thread in the mean time, but this will allow
	 * you to prevent calls to ftl::TaskScheduler::GetCurrentThreadIndex() every single time you want to modify the object (similar to TLS optimization).
	 * This is the recommended way of doing that over getting a reference as it allows instrumentation and debugging within the library.
	 *
	 * @return    Handle to the thread's version of T.
	 */
	ThreadLocalHandle<T> GetHandle() {
		return ThreadLocalHandle<T>{*this, **this};
	}

	T& operator*() {
		std::size_t idx = m_scheduler->GetCurrentThreadIndex();
		if(!m_data[idx].m_inited) {
			new(&m_data[idx].m_value) T(m_initalizer());
			m_data[idx].m_inited = true;
		}
		return m_data[idx].m_value;
	}
	T* operator->() {
		std::size_t idx = m_scheduler->GetCurrentThreadIndex();
		if(!m_data[idx].m_inited) {
			new(&m_data[idx].m_value) T(m_initalizer());
			m_data[idx].m_inited = true;
		}
		return &m_data[idx].m_value;
	}
private:
	TaskScheduler* m_scheduler;
	std::function<T()> m_initalizer;
	ValuePadder<T>* m_data;
};

#if FTL_THREAD_LOCAL_HANDLE_DEBUG
template<class T>
void ThreadLocalHandle<T>::valid_handle(T& value) {
	if (&*m_parent != &value) {
		assert(!"Invalid ThreadLocalHandle");
	};
}
#else
template<class T>
bool ThreadLocalHandle<T>::valid_handle(T& value) {}
#endif

// C++17 deduction guide
#ifdef __cpp_deduction_guides
template<class T> ThreadLocal(TaskScheduler*, T) -> ThreadLocal<T>;
#endif
}
