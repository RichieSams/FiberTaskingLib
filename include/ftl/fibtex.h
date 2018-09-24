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

#include "task_scheduler.h"
#include "atomic_counter.h"


namespace ftl {
/**
 * A fiber aware mutex. Does not block in the traditional way. Methods do not follow the lowerCamelCase convention
 * of the rest of the library in order to comply with C++'s named requirement Basic Lockable and Lockable.
 */
class Fibtex {
public:
	/**
	 * All Fibtex's have to be aware of the task scheduler in order to yield.
	 *
	 * @param taskScheduler    ftl::TaskScheduler that will be using this mutex.
	 */
	explicit Fibtex(ftl::TaskScheduler *taskScheduler)
		: m_taskScheduler(taskScheduler),
		  m_atomicCounter(taskScheduler, 0),
		  m_ableToSpin(taskScheduler->GetThreadCount() > 1) {}

	/**
	 * Lock mutex in traditional way, yielding immediately.
	 */
	void lock() {
		while (true) {
			if (m_atomicCounter.CompareExchange(0, 1)) {
				return;
			}

			m_taskScheduler->WaitForCounter(&m_atomicCounter, 0);
		}
	}

	/**
	 * Lock mutex using an infinite spinlock. Does not spin if there is only one backing thread.
	 */
	void lock_spin() {
		// Don't spin if there is only one thread and spinning is pointless
		if (!m_ableToSpin) {
			lock();
			return;
		}

		while (true) {
			// Spin
			if (m_atomicCounter.CompareExchange(0, 1)) {
				return;
			}
			FTL_PAUSE();
		}
	}

	/**
	 * Lock mutex using an finite spinlock. Does not spin if there is only one backing thread.
	 *
	 * @param iterations    Times to spin.
	 */
	void lock_spin_iter(uint iterations = 1000) {
		// Don't spin if there is only one thread and spinning is pointless
		if (!m_ableToSpin) {
			lock();
			return;
		}

		// Spin for a bit
		for (uint i = 0; i < iterations; ++i) {
			// Spin
			if (m_atomicCounter.CompareExchange(0, 1)) {
				return;
			}
			FTL_PAUSE();
		}

		// Spinning didn't grab the lock, we're in for the long hall. Yield.
		lock();
	}

	/**
	 * Attempts to lock the lock a single time.
	 *
	 * @return    If lock successful.
	 */
	bool try_lock() {
		return m_atomicCounter.CompareExchange(0, 1);
	}

	/**
	 * Unlock the mutex.
	 */
	void unlock() {
		if (!m_atomicCounter.CompareExchange(1, 0)) {
			printf("Mutex was unlocked by another fiber or was double unlocked.\n");
			return;
		}
	}

private:
	bool m_ableToSpin = false;
	ftl::TaskScheduler *m_taskScheduler;
	ftl::AtomicCounter m_atomicCounter;
};
}
