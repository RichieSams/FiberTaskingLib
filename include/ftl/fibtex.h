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

#include "ftl/atomic_counter.h"
#include "ftl/task_scheduler.h"

#include <cassert>
#include <mutex>
#include <system_error>

namespace ftl {
// Pull std helpers into ftl namespace
using std::adopt_lock;
using std::defer_lock;
using std::try_to_lock;

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
	 * @param fiberSlots       How many fibers can simultaneously wait on the mutex
	 */
	explicit Fibtex(TaskScheduler *taskScheduler, unsigned fiberSlots = NUM_WAITING_FIBER_SLOTS)
	        : m_ableToSpin(taskScheduler->GetThreadCount() > 1), m_taskScheduler(taskScheduler),
	          m_atomicCounter(taskScheduler, 0, fiberSlots) {
	}

	Fibtex(const Fibtex &) = delete;
	Fibtex(Fibtex &&that) = delete;
	Fibtex &operator=(const Fibtex &) = delete;
	Fibtex &operator=(Fibtex &&that) = delete;

	~Fibtex() = default;

private:
	bool m_ableToSpin;
	TaskScheduler *m_taskScheduler;
	AtomicFlag m_atomicCounter;

public:
	/**
	 * Lock mutex in traditional way, yielding immediately.
	 */
	// ReSharper disable once CppInconsistentNaming
	void lock(bool const pinToThread = false) {
		while (true) {
			if (m_atomicCounter.Set(std::memory_order_acq_rel)) {
				return;
			}

			m_taskScheduler->WaitForCounter(&m_atomicCounter, pinToThread);
		}
	}

	/**
	 * Lock mutex using a finite spinlock. Does not spin if there is only one backing thread.
	 *
	 * @param pinToThread    If the fiber should resume on the same thread as it started on pre-lock.
	 * @param iterations     Amount of iterations to spin before yielding.
	 */
	// ReSharper disable once CppInconsistentNaming
	void lock_spin(bool const pinToThread = false, unsigned const iterations = 1000) {
		// Don't spin if there is only one thread and spinning is pointless
		if (!m_ableToSpin) {
			lock(pinToThread);
			return;
		}

		// Spin for a bit
		for (unsigned i = 0; i < iterations; ++i) {
			// Spin
			if (m_atomicCounter.Set(std::memory_order_acq_rel)) {
				return;
			}
			FTL_PAUSE();
		}

		// Spinning didn't grab the lock, we're in for the long haul. Yield.
		lock(pinToThread);
	}

	/**
	 * Lock mutex using an infinite spinlock. Does not spin if there is only one backing thread.
	 */
	// ReSharper disable once CppInconsistentNaming
	void lock_spin_infinite(bool const pinToThread = false) {
		// Don't spin if there is only one thread and spinning is pointless
		if (!m_ableToSpin) {
			lock(pinToThread);
			return;
		}

		while (true) {
			// Spin
			if (m_atomicCounter.Set(std::memory_order_acq_rel)) {
				return;
			}
			FTL_PAUSE();
		}
	}

	/**
	 * Attempts to lock the lock a single time.
	 *
	 * @return    If lock successful.
	 */
	// ReSharper disable once CppInconsistentNaming
	bool try_lock() {
		return m_atomicCounter.Set(std::memory_order_acq_rel);
	}

	/**
	 * Unlock the mutex.
	 */
	// ReSharper disable once CppInconsistentNaming
	void unlock() {
		if (!m_atomicCounter.Clear(std::memory_order_acq_rel)) {
			FTL_ASSERT("Error: Mutex was unlocked by another fiber or was double unlocked.", false);
		}
	}
};

enum class FibtexLockBehavior {
	Traditional,
	Spin,
	SpinInfinite,
};

/**
 * A wrapper class that encapsulates pinToThread, and the various spin behaviors
 */
class LockWrapper {
public:
	/**
	 * @param mutex             Fibtex to wrap around.
	 * @param behavior          How to lock the underlying Fibtex
	 * @param pinToThread       If the fiber should resume on the same thread as it started on pre-lock.
	 * @param spinIterations    If behavior == Spin, this gives the amount of iterations to spin before yielding. Ignored otherwise.
	 */
	LockWrapper(Fibtex &mutex, FibtexLockBehavior behavior, bool pinToThread = false, unsigned spinIterations = 1000)
	        : m_mutex(mutex), m_pinToThread(pinToThread), m_behavior(behavior), m_spinIterations(spinIterations) {
	}
	/**
	 * Locks mutex
	 */
	void lock() {
		switch (m_behavior) {
		case FibtexLockBehavior::Traditional:
		default:
			m_mutex.lock(m_pinToThread);
			break;
		case FibtexLockBehavior::Spin:
			m_mutex.lock_spin(m_pinToThread, m_spinIterations);
			break;
		case FibtexLockBehavior::SpinInfinite:
			m_mutex.lock_spin_infinite(m_pinToThread);
			break;
		}
	}
	/**
	 * Tries to lock mutex
	 *
	 * @return    If mutex successfully locked.
	 */
	bool try_lock() {
		return m_mutex.try_lock();
	}
	/**
	 * Unlocks mutex.
	 */
	void unlock() const {
		m_mutex.unlock();
	}

private:
	Fibtex &m_mutex;
	bool m_pinToThread;
	FibtexLockBehavior m_behavior;
	unsigned m_spinIterations;
};

} // namespace ftl
