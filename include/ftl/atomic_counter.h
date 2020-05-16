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

#include <atomic>
#include <limits>
#include <thread>
#include <vector>

namespace ftl {

class TaskScheduler;

/**
 * AtomicCounter is a wrapper over a C++11 atomic_unsigned
 * In FiberTaskingLib, AtomicCounter is used to create dependencies between Tasks, and
 * is how you wait for a set of Tasks to finish.
 */
class AtomicCounter {

/**
 * This value defines how many fibers can simultaneously wait on a counter
 * If a user tries to have more fibers wait on a counter, the fiber will not be tracked,
 * which will cause WaitForCounter() to infinitely sleep. This will probably cause a hang.
 *
 * Do NOT set this by definition either in your build or before you include the header.
 * This macro must be the same value throughout the whole build. Update the value within
 * cmake using the following:
 *
 * set(FTL_NUM_WAITING_FIBER_SLOTS <value> CACHE STRING "Number of slots within ftl::AtomicCounter for fibers to wait" FORCE)
 */
#ifndef NUM_WAITING_FIBER_SLOTS
#	define NUM_WAITING_FIBER_SLOTS 4
#endif

public:
	explicit AtomicCounter(TaskScheduler *taskScheduler, unsigned const initialValue = 0, size_t const fiberSlots = NUM_WAITING_FIBER_SLOTS);

	AtomicCounter(AtomicCounter const &) = delete;
	AtomicCounter(AtomicCounter &&) noexcept = delete;
	AtomicCounter &operator=(AtomicCounter const &) = delete;
	AtomicCounter &operator=(AtomicCounter &&) noexcept = delete;
	~AtomicCounter();

private:
	/* The TaskScheduler this counter is associated with */
	TaskScheduler *m_taskScheduler;
	/* The atomic counter holding our data */
	std::atomic<unsigned> m_value;
	/* An atomic counter to ensure the instance can't be destroyed while other threads are still inside a function */
	std::atomic<unsigned> m_lock;
	/**
	 * An array that signals which slots in m_waitingFibers are free to be used
	 * True: Free
	 * False: Full
	 *
	 * We can't use a vector for m_freeSlots because std::atomic<t> is non-copyable and
	 * non-moveable. std::vector constructor, push_back, and emplace_back all use either
	 * copy or move
	 */
	std::atomic<bool> *m_freeSlots;

	struct WaitingFiberBundle {
		WaitingFiberBundle();

		/**
		 * A bundle is "InUse" when it's being filled with data, or when it's being removed
		 * The wait checking code uses this atomic to prevent multiple threads from modifying the same slot
		 */
		std::atomic<bool> InUse;
		/* The fiber bundle that's waiting */
		void *FiberBundle{nullptr};
		/* The value the fiber is waiting for */
		unsigned TargetValue{0};
		/**
		 * The index of the thread this fiber is pinned to
		 * If the fiber *isn't* pinned, this will equal std::numeric_limits<size_t>::max()
		 */
		size_t PinnedThreadIndex;
	};
	/**
	 * The storage for the fibers waiting on this counter
	 *
	 * We again can't use a vector because WaitingFiberBundle contains a std::atomic<t>, which is is non-copyable and
	 * non-moveable. std::vector constructor, push_back, and emplace_back all use either copy or move
	 */
	WaitingFiberBundle *m_waitingFibers;

	/**
	 * The number of elements in m_freeSlots and m_waitingFibers
	 */
	size_t m_fiberSlots;

	/**
	 * We friend TaskScheduler so we can keep AddFiberToWaitingList() private
	 * This makes the public API cleaner
	 */
	friend class TaskScheduler;

public:
	/**
	 * A wrapper over std::atomic_unsigned::load()
	 *
	 * The load *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param memoryOrder    The memory order to use for the load
	 * @return               The current value of the counter
	 */
	unsigned Load(std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		unsigned ret = m_value.load(memoryOrder);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return ret;
	}
	/**
	 * A wrapper over std::atomic_unsigned::store()
	 *
	 * The store *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to load into the counter
	 * @param memoryOrder    The memory order to use for the store
	 */
	void Store(unsigned const x, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		m_value.store(x, memoryOrder);
		CheckWaitingFibers(x);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
	}
	/**
	 * A wrapper over std::atomic_unsigned::fetch_add()
	 *
	 * The fetch_add *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to add to the counter
	 * @param memoryOrder    The memory order to use for the fetch_add
	 * @return               The value of the counter before the addition
	 */
	unsigned FetchAdd(unsigned const x, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		const unsigned prev = m_value.fetch_add(x, memoryOrder);
		CheckWaitingFibers(prev + x);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return prev;
	}
	/**
	 * A wrapper over std::atomic_unsigned::fetch_sub()
	 *
	 * The fetch_sub *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to subtract from the counter
	 * @param memoryOrder    The memory order to use for the fetch_sub
	 * @return               The value of the counter before the subtraction
	 */
	unsigned FetchSub(unsigned const x, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		const unsigned prev = m_value.fetch_sub(x, memoryOrder);
		CheckWaitingFibers(prev - x);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return prev;
	}

	/**
	 * A wrapper over std::atomic_unsigned::compare_exchange_strong()
	 *
	 * The compare_exchange_strong *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param expectedValue    The value that is expected to be in the atomic counter
	 * @param newValue         The value that the atomic counter will be set to if comparison succeeds.
	 * @param memoryOrder      The memory order to use for the compare_exchange_strong
	 * @return                 If the compare_exchange_strong succeeded
	 */
	bool CompareExchange(unsigned expectedValue, unsigned const newValue, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		bool const success = m_value.compare_exchange_strong(expectedValue, newValue, memoryOrder);
		if (success) {
			CheckWaitingFibers(newValue);
		}

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return success;
	}

private:
	/**
	 * Add a fiber to the list of waiting fibers
	 *
	 * NOTE: Called by TaskScheduler from inside WaitForCounter
	 *
	 * WARNING: The end-user must guarantee that the number of simultaneous waiting fibers is
	 * less than or equal to NUM_WAITING_FIBER_SLOTS. If a user tries to have more fibers wait
	 * on a counter, the fiber will not be tracked, which will cause WaitForCounter() to
	 * infinitely sleep. This will probably cause a hang
	 *
	 * @param targetValue          The target value the fiber is waiting for
	 * @param fiberBundle          The fiber that is waiting
	 * @param pinnedThreadIndex    The index of the thread this fiber is pinned to. If == std::numeric_limits<size_t>::max(), the fiber can be resumed on any thread
	 * @return                     True: The counter value changed to equal targetValue while we were adding the fiber to the wait list
	 */
	bool AddFiberToWaitingList(void *fiberBundle, unsigned targetValue, size_t pinnedThreadIndex = std::numeric_limits<size_t>::max());

	/**
	 * Checks all the waiting fibers in the list to see if value == targetValue
	 * If it finds one, it removes it from the list, and signals the
	 * TaskScheduler to add it to its ready task list
	 *
	 * @param value    The value to check
	 */
	void CheckWaitingFibers(unsigned value);
};

} // End of namespace ftl
