/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
*
* This library was created as a proof of concept of the ideas presented by
* Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
*
* http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
*
* FiberTaskingLib is the legal property of Adrian Astley
* Copyright Adrian Astley 2015 - 2016
*/

/**
 * This is an implementation of 'Correct and Efficient Work-Stealing for Weak Memory Models' by Lê et. al [2013]
 * 
 * https://hal.inria.fr/hal-00802885
 */

#pragma once

#include "fiber_tasking_lib/typedefs.h"

#include <atomic>
#include <vector>
#include <memory>
#include <cassert>


namespace FiberTaskingLib {

template<typename T>
class WaitFreeQueue {
public:
	WaitFreeQueue()
		: m_top(1), // m_top and m_bottom must start at 1
		  m_bottom(1), // Otherwise, the first Pop on an empty queue will underflow m_bottom
		  m_array(new CircularArray(32)) {
	}
	~WaitFreeQueue() {
		delete m_array.load(std::memory_order_relaxed);
	}

private:
	class CircularArray {
	public:
		CircularArray(std::size_t n)
				: items(n) {
			assert(!(n == 0) && !(n & (n - 1)) && "n must be a power of 2");
		}

	private:
		std::vector<T> items;
		std::unique_ptr<CircularArray> previous;

	public:
		std::size_t Size() const {
			return items.size();
		}

		T Get(std::size_t index) {
			return items[index & (Size() - 1)];
		}

		void Put(std::size_t index, T x) {
			items[index & (Size() - 1)] = x;
		}

		// Growing the array returns a new circular_array object and keeps a
		// linked list of all previous arrays. This is done because other threads
		// could still be accessing elements from the smaller arrays.
		CircularArray *Grow(std::size_t top, std::size_t bottom) {
			CircularArray *new_array = new CircularArray(Size() * 2);
			new_array->previous.reset(this);
			for (std::size_t i = top; i != bottom; i++) {
				new_array->Put(i, Get(i));
			}
			return new_array;
		}
	};

	std::atomic<uint64> m_top;
	// Cache-line pad
	char pad[64];
	std::atomic<uint64> m_bottom;
	// Cache-line pad
	char pad2[64];
	std::atomic<CircularArray *> m_array;


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
		CircularArray *array = m_array.load(std::memory_order_relaxed);
		m_bottom.store(b, std::memory_order_relaxed);

		std::atomic_thread_fence(std::memory_order_seq_cst);

		uint64 t = m_top.load(std::memory_order_relaxed);
		bool result = true;
		if (t <= b) {
			/* Non-empty queue. */
			*value = array->Get(b);
			if (t == b) {
				/* Single last element in queue. */
				if (!std::atomic_compare_exchange_strong_explicit(&m_top, &t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
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

	bool Steal(T *value) {
		uint64 t = m_top.load(std::memory_order_acquire);

		#if defined(FTL_STRONG_MEMORY_MODEL)
			std::atomic_signal_fence(std::memory_order_seq_cst);
		#else
			std::atomic_thread_fence(std::memory_order_seq_cst);
		#endif

		uint64 b = m_bottom.load(std::memory_order_acquire);
		if (t < b) {
			/* Non-empty queue. */
			CircularArray *array = m_array.load(std::memory_order_consume);
			*value = array->Get(t);
			if (!std::atomic_compare_exchange_strong_explicit(&m_top, &t, t + 1, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				/* Failed race. */
				return false;
			}

			return true;
		}
		
		return false;
	}
};

} // End of namespace FiberTaskingLib
