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

namespace ftl {

class TaskScheduler;

/**
 * This value defines how many fibers can simultaneously wait on a counter
 * If a user tries to have more fibers wait on a counter, the fiber will not be tracked,
 * which will cause WaitForCounter() to infinitely sleep. This will probably cause a hang.
 *
 * Do NOT set this by definition either in your build or before you include the header.
 * This macro must be the same value throughout the whole build. Update the value within
 * cmake using the following:
 *
 * set(FTL_NUM_WAITING_FIBER_SLOTS <value> CACHE STRING "Number of slots within ftl::TaskCounter for fibers to wait" FORCE)
 */
#ifndef NUM_WAITING_FIBER_SLOTS
#	define NUM_WAITING_FIBER_SLOTS 4
#endif

/**
 * BaseCounter is a wrapper over a C++11 atomic_unsigned
 * It implements the base logic for the rest of the AtomicCounter-type classes
 *
 * You should never use this class directly. Use the other AtomicCounter-type classes
 */
class BaseCounter {

public:
	/**
	 * Creates a BaseCounter
	 *
	 * @param taskScheduler    The TaskScheduler this flag references
	 * @param initialValue     The initial value of the flag
	 * @param fiberSlots       This defines how many fibers can wait on this counter.
	 *                         If fiberSlots == NUM_WAITING_FIBER_SLOTS, this constructor will *not* allocate memory
	 */
	explicit BaseCounter(TaskScheduler *taskScheduler, unsigned initialValue = 0, unsigned fiberSlots = NUM_WAITING_FIBER_SLOTS);

	BaseCounter(BaseCounter const &) = delete;
	BaseCounter(BaseCounter &&) noexcept = delete;
	BaseCounter &operator=(BaseCounter const &) = delete;
	BaseCounter &operator=(BaseCounter &&) noexcept = delete;
	~BaseCounter();

protected:
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
	 *
	 * We use Small Vector Optimization in order to avoid allocations for most cases
	 */
	std::atomic<bool> *m_freeSlots;
	std::atomic<bool> m_freeSlotsStorage[NUM_WAITING_FIBER_SLOTS];

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
		 * If the fiber *isn't* pinned, this will equal std::numeric_limits<unsigned>::max()
		 */
		unsigned PinnedThreadIndex;
	};
	/**
	 * The storage for the fibers waiting on this counter
	 *
	 * We again can't use a vector because WaitingFiberBundle contains a std::atomic<t>, which is is non-copyable and
	 * non-moveable. std::vector constructor, push_back, and emplace_back all use either copy or move
	 *
	 * We also use Small Vector Optimization in order to avoid allocations for most cases
	 */
	WaitingFiberBundle *m_waitingFibers;
	WaitingFiberBundle m_waitingFibersStorage[NUM_WAITING_FIBER_SLOTS];

	/**
	 * The number of elements in m_freeSlots and m_waitingFibers
	 */
	unsigned m_fiberSlots;

	/**
	 * We friend TaskScheduler so we can keep AddFiberToWaitingList() private
	 * This makes the public API cleaner
	 */
	friend class TaskScheduler;

protected:
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
	 * @param pinnedThreadIndex    The index of the thread this fiber is pinned to. If == std::numeric_limits<unsigned>::max(), the fiber can be resumed on any thread
	 * @return                     True: The counter value changed to equal targetValue while we were adding the fiber to the wait list
	 */
	bool AddFiberToWaitingList(void *fiberBundle, unsigned targetValue, unsigned pinnedThreadIndex = std::numeric_limits<unsigned>::max());

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
