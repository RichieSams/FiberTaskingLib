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
#include <inttypes.h>

namespace ftl {

class TaskScheduler;

/**
 * WaitGroup is used to track how many tasks are yet to be finished
 * It is used to create dependencies between Tasks, and is how you wait
 * for a set of Tasks to finish.
 */
class WaitGroup {

public:
	/**
	 * @brief Creates a WaitGroup
	 *
	 * @param taskScheduler    The TaskScheduler to use with this WaitGroup
	 */
	explicit WaitGroup(TaskScheduler *taskScheduler);

	WaitGroup(WaitGroup const &) = delete;
	WaitGroup(WaitGroup &&) noexcept = delete;
	WaitGroup &operator=(WaitGroup const &) = delete;
	WaitGroup &operator=(WaitGroup &&) noexcept = delete;

	~WaitGroup() = default;

private:
	/* The TaskScheduler this WaitGroup is associated with */
	TaskScheduler *m_taskScheduler;

	/* The counter that can be waited on. Once it is zero, all waiters will be released */
	std::atomic<int32_t> m_counter;
	/* We store the queue lock and the queue all in a single uintptr_t */
	std::atomic<uintptr_t> m_word;

	static constexpr uintptr_t kIsQueueLockedBit = 1;
	static constexpr uintptr_t kQueueHeadMask = 1;

public:
	/**
	 * @brief Adds the value, which may be negative, to the counter
	 *
	 * If the counter becomes zero, all fibers blocked on Wait are released
	 *
	 * NOTE: Add() *must not* be called after *any* thread has called Wait(). The resulting
	 *       behavior is undefined
	 *
	 * NOTE: If a WaitGroup is re-used to wait for several independent sets of events, new
	 *       calls to Add() must happen *after* all previous Wait() calls have returned
	 *
	 * @param delta    The value to add to the counter
	 */
	void Add(int32_t delta);

	/**
	 * @brief Decrement the counter by 1
	 */
	void Done() {
		Add(-1);
	}

	/**
	 * @brief Wait blocks until the WaitGroup counter is zero
	 *
	 * @param pinToCurrentThread    If true, this fiber won't be resumed on another thread
	 */
	void Wait(bool pinToCurrentThread = false);
};

} // End of namespace ftl
