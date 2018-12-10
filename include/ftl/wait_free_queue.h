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

/**
 * This is an implementation of 'Correct and Efficient Work-Stealing for Weak Memory Models' by Le et. al [2013]
 *
 * https://hal.inria.fr/hal-00802885
 */

#pragma once

#include "ftl/assert.h"
#include "ftl/typedefs.h"

#include <atomic>
#include <memory>
#include <vector>

namespace ftl {

template <typename T>
class WaitFreeQueue {
private:
	constexpr static std::size_t kStartingCircularArraySize = 32;

public:
	WaitFreeQueue()
	        : m_top(1),    // m_top and m_bottom must start at 1
	          m_bottom(1), // Otherwise, the first Pop on an empty queue will underflow m_bottom
	          m_array(new CircularArray(kStartingCircularArraySize)) {
		FTL_VALGRIND_HG_DISABLE_CHECKING(&m_top, sizeof(m_top));
		FTL_VALGRIND_HG_DISABLE_CHECKING(&m_bottom, sizeof(m_bottom));
		FTL_VALGRIND_HG_DISABLE_CHECKING(&m_array, sizeof(m_array));
	}

	WaitFreeQueue(WaitFreeQueue const &) = delete;
	WaitFreeQueue(WaitFreeQueue &&) noexcept = delete;
	WaitFreeQueue &operator=(WaitFreeQueue const &) = delete;
	WaitFreeQueue &operator=(WaitFreeQueue &&) noexcept = delete;
	~WaitFreeQueue() {
		delete m_array.load(std::memory_order_relaxed);
	}

private:
	class CircularArray {
	public:
		explicit CircularArray(std::size_t const n) : m_items(n) {
			FTL_ASSERT("n must be a power of 2", !(n == 0) && !(n & (n - 1)));
		}

	private:
		std::vector<T> m_items;
		std::unique_ptr<CircularArray> m_previous;

	public:
		std::size_t Size() const {
			return m_items.size();
		}

		T Get(std::size_t const index) {
			return m_items[index & (Size() - 1)];
		}

		void Put(std::size_t const index, T x) {
			m_items[index & (Size() - 1)] = x;
		}

		// Growing the array returns a new circular_array object and keeps a
		// linked list of all previous arrays. This is done because other threads
		// could still be accessing elements from the smaller arrays.
		CircularArray *Grow(std::size_t const top, std::size_t const bottom) {
			auto *const newArray = new CircularArray(Size() * 2);
			newArray->m_previous.reset(this);
			for (std::size_t i = top; i != bottom; i++) {
				newArray->Put(i, Get(i));
			}
			return newArray;
		}
	};

	alignas(kCacheLineSize) std::atomic<uint64> m_top;
	alignas(kCacheLineSize) std::atomic<uint64> m_bottom;
	alignas(kCacheLineSize) std::atomic<CircularArray *> m_array;

public:
	void Push(T value) {
		uint64 b = m_bottom.load(std::memory_order_relaxed);
		uint64 t = m_top.load(std::memory_order_acquire);
		CircularArray *array = m_array.load(std::memory_order_relaxed);

		if (b - t > array->Size() - 1) {
			/* Full queue. */
			array = array->Grow(t, b);
			m_array.store(array, std::memory_order_release);
		}
		array->Put(b, value);

#if defined(FTL_STRONG_MEMORY_MODEL)
		std::atomic_signal_fence(std::memory_order_release);
#else
		std::atomic_thread_fence(std::memory_order_release);
#endif

		m_bottom.store(b + 1, std::memory_order_relaxed);
	}

	bool Pop(T *value) {
		uint64 b = m_bottom.load(std::memory_order_relaxed) - 1;
		CircularArray *const array = m_array.load(std::memory_order_relaxed);
		m_bottom.store(b, std::memory_order_relaxed);

		std::atomic_thread_fence(std::memory_order_seq_cst);

		uint64 t = m_top.load(std::memory_order_relaxed);
		bool result = true;
		if (t <= b) {
			/* Non-empty queue. */
			*value = array->Get(b);
			if (t == b) {
				/* Single last element in queue. */
				if (!std::atomic_compare_exchange_strong_explicit(&m_top, &t, t + 1, std::memory_order_seq_cst,
				                                                  std::memory_order_relaxed)) {
					/* Failed race. */
					result = false;
				}
				m_bottom.store(b + 1, std::memory_order_relaxed);
			}
		} else {
			/* Empty queue. */
			result = false;
			m_bottom.store(b + 1, std::memory_order_relaxed);
		}

		return result;
	}

	bool Steal(T *const value) {
		uint64 t = m_top.load(std::memory_order_acquire);

#if defined(FTL_STRONG_MEMORY_MODEL)
		std::atomic_signal_fence(std::memory_order_seq_cst);
#else
		std::atomic_thread_fence(std::memory_order_seq_cst);
#endif

		uint64 const b = m_bottom.load(std::memory_order_acquire);
		if (t < b) {
			/* Non-empty queue. */
			CircularArray *const array = m_array.load(std::memory_order_consume);
			*value = array->Get(t);
			return std::atomic_compare_exchange_strong_explicit(&m_top, &t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed);
		}

		return false;
	}
};

} // End of namespace ftl
