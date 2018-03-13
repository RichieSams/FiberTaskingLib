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

#include "ftl/typedefs.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>


namespace ftl {

class TaskScheduler;

/** 
 * AtomicCounter is a wrapper over a C++11 atomic_uint
 * In FiberTaskingLib, AtomicCounter is used to create dependencies between Tasks, and
 * is how you wait for a set of Tasks to finish.
 */
class AtomicCounter {

/**
 * This value defines how many fibers can simultaneously wait on a counter 
 * If a user tries to have more fibers wait on a counter, the fiber will not be tracked,
 * which will cause WaitForCounter() to infinitely sleep. This will probably cause a hang
 */
#define NUM_WAITING_FIBER_SLOTS 4

public:
	AtomicCounter(TaskScheduler *taskScheduler, uint initialValue = 0) 
			: m_taskScheduler(taskScheduler),
			  m_value(initialValue) {
		for (uint i = 0; i < NUM_WAITING_FIBER_SLOTS; ++i) {
			m_freeSlots[i].store(true);
			// We initialize InUse to true to prevent CheckWaitingFibers() from checking garbage
			// data when we are adding a new fiber to the wait list in AddFiberToWaitingList()
			// For this same reason, when we set a slot to be free (ie. m_freeSlots[i] = true), we
			// keep InUse == true
			m_waitingFibers[i].InUse.store(true);
		}

		m_lock.store(0);
	}

private:
	/* The TaskScheduler this counter is associated with */
	TaskScheduler *m_taskScheduler;
	/* The atomic counter holding our data */
	std::atomic_uint m_value;
	/* An atomic counter to ensure threads don't race in readying the fibers */
	std::atomic_uint m_lock;
	/* An array that signals which slots in m_waitingFibers are free to be used
	 * True: Free
	 * False: Full
	 */
	std::atomic<bool> m_freeSlots[NUM_WAITING_FIBER_SLOTS];

	struct WaitingFiberBundle {
		WaitingFiberBundle()
				: InUse(true),
				  FiberIndex(0),
				  TargetValue(0), 
				  FiberStoredFlag(nullptr),
				  PinnedThreadIndex(0) {
		}

		/** 
		 * A bundle is "InUse" when it's being filled with data, or when it's being removed
		 * The wait checking code uses this atomic to prevent multiple threads from modifying the same slot
		 */
		std::atomic<bool> InUse;
		/* The index of the fiber that is waiting on this counter */
		std::size_t FiberIndex;
		/* The value the fiber is waiting for */
		uint TargetValue;
		/** 
		 * A flag signaling if the fiber has been successfully switched out of and "cleaned up"
		 * See TaskScheduler::CleanUpOldFiber()
		 */
		std::atomic<bool> *FiberStoredFlag;
		/**
		 * The index of the thread this fiber is pinned to
		 * If the fiber *isn't* pinned, this will equal std::numeric_limits<std::size_t>::max()
		 */
		std::size_t PinnedThreadIndex;
	};
	WaitingFiberBundle m_waitingFibers[NUM_WAITING_FIBER_SLOTS];

	/**
	* We friend TaskScheduler so we can keep AddFiberToWaitingList() private
	* This makes the public API cleaner
	*/
	friend class TaskScheduler;

public:
	/**
	 * A wrapper over std::atomic_uint::load()
	 * 
	 * The load *will* be atomic, but this function as a whole is *not* atomic
	 * 
	 * @param memoryOrder    The memory order to use for the load
	 * @return               The current value of the counter
	 */
	uint Load(std::memory_order memoryOrder = std::memory_order_seq_cst) {
		return m_value.load(memoryOrder);
	}
	/**
	 * A wrapper over std::atomic_uint::store()
	 * 
	 * The store *will* be atomic, but this function as a whole is *not* atomic
	 * 
	 * @param x              The value to load into the counter
	 * @param memoryOrder    The memory order to use for the store
	 */
	void Store(uint x, std::memory_order memoryOrder = std::memory_order_seq_cst) {
		// Enter shared section
		m_lock.fetch_add(1u, std::memory_order_seq_cst);
		m_value.store(x, memoryOrder);
		CheckWaitingFibers(x);
	}
	/**
	 * A wrapper over std::atomic_uint::fetch_add()
	 * 
	 * The fetch_add *will* be atomic, but this function as a whole is *not* atomic
	 * 
	 * @param x              The value to add to the counter
	 * @param memoryOrder    The memory order to use for the fetch_add
	 * @return               The value of the counter before the addition
	 */
	uint FetchAdd(uint x, std::memory_order memoryOrder = std::memory_order_seq_cst) {
		// Enter shared section
		m_lock.fetch_add(1u, std::memory_order_seq_cst);
		const uint prev = m_value.fetch_add(x, memoryOrder);
		CheckWaitingFibers(prev + x);

		return prev;
	}
	/**
	 * A wrapper over std::atomic_uint::fetch_sub()
	 *
	 * The fetch_sub *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to subtract from the counter
	 * @param memoryOrder    The memory order to use for the fetch_sub
	 * @return               The value of the counter before the subtraction
	 */
	uint FetchSub(uint x, std::memory_order memoryOrder = std::memory_order_seq_cst) {
		// Enter shared section
		m_lock.fetch_add(1u, std::memory_order_seq_cst);
		const uint prev = m_value.fetch_sub(x, memoryOrder);
		CheckWaitingFibers(prev - x);

		return prev;
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
	 * @param fiberIndex         The index of the fiber that is waiting
	 * @param targetValue        The target value the fiber is waiting for
	 * @param fiberStoredFlag    A flag used to signal if the fiber has been successfully switched out of and "cleaned up"
	 * @return                   True: The counter value changed to equal targetValue while we were adding the fiber to the wait list
	 */
	bool AddFiberToWaitingList(std::size_t fiberIndex, uint targetValue, std::atomic<bool> *fiberStoredFlag, std::size_t pinnedThreadIndex = std::numeric_limits<std::size_t>::max());

	/**
	 * Checks all the waiting fibers in the list to see if value == targetValue
	 * If it finds one, it removes it from the list, and signals the 
	 * TaskScheduler to add it to its ready task list
	 *
	 * @param value    The value to check
	 */
	void CheckWaitingFibers(uint value);
};

} // End of namespace ftl

