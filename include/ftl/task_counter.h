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
	  * AtomicCounter is a wrapper over a C++11 atomic_unsigned
	  * In FiberTaskingLib, AtomicCounter is used to create dependencies between Tasks, and
	  * is how you wait for a set of Tasks to finish.
	  */
class TaskCounter {

public:
	explicit TaskCounter(TaskScheduler *taskScheduler, unsigned const initialValue = 0, size_t const fiberSlots = NUM_WAITING_FIBER_SLOTS);

	TaskCounter(TaskCounter const &) = delete;
	TaskCounter(TaskCounter &&) noexcept = delete;
	TaskCounter &operator=(TaskCounter const &) = delete;
	TaskCounter &operator=(TaskCounter &&) noexcept = delete;
	virtual ~TaskCounter();

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

protected:
	/**
		 * Adds the value to the counter. It will not check any waiting tasks, since it is assumed they're waiting
		 * for a final value of 0.
		 *
		 * @param x    The value to add to the counter
		 */
	virtual void Add(unsigned const x);

	/**
		 * Decrement the counter by 1. If the new value would be zero, it will resume the waiting tasks.
		 */
	virtual void Decrement();

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
