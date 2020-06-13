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

#include "base_counter.h"

namespace ftl {

class TaskScheduler;

/**
 * TaskCounter is used to track how many tasks are yet to be finished
 * It is used to create dependencies between Tasks, and is how you wait
 * for a set of Tasks to finish.
 */
class TaskCounter : public BaseCounter {

public:
	explicit TaskCounter(TaskScheduler *taskScheduler, unsigned const initialValue = 0, size_t const fiberSlots = NUM_WAITING_FIBER_SLOTS)
	        : BaseCounter(taskScheduler, initialValue, fiberSlots) {
	}

	TaskCounter(TaskCounter const &) = delete;
	TaskCounter(TaskCounter &&) noexcept = delete;
	TaskCounter &operator=(TaskCounter const &) = delete;
	TaskCounter &operator=(TaskCounter &&) noexcept = delete;

public:
	/**
	 * Adds the value to the counter. It will not check any waiting tasks, since it is assumed they're waiting
	 * for a final value of 0.
	 *
	 * @param x    The value to add to the counter
	 */
	void Add(unsigned const x) {
		m_value.fetch_add(x, std::memory_order_seq_cst);
	}

	/**
	 * Decrement the counter by 1. If the new value would be zero, it will resume the waiting tasks.
	 */
	void Decrement() {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		const unsigned prev = m_value.fetch_sub(1U, std::memory_order_seq_cst);
		const unsigned newValue = prev - 1;

		// TaskCounters are only allowed to wait on 0, so we only need to check when newValue would be zero
		if (newValue == 0) {
			CheckWaitingFibers(newValue);
		}

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
	}
};

} // End of namespace ftl
