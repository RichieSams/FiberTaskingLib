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

namespace ftl {

class TaskScheduler;

/**
 * A fiber aware mutex. Does not block in the traditional way. Methods do not follow the lowerCamelCase convention
 * of the rest of the library in order to comply with C++'s named requirement Basic Lockable and Lockable.
 */
class Fibtex {
public:
	/**
	 * All Fibtex's have to be aware of the task scheduler in order to yield.
	 *
	 * @param taskScheduler    The TaskScheduler that will be using this mutex.
	 */
	explicit Fibtex(TaskScheduler *taskScheduler);

	Fibtex(const Fibtex &) = delete;
	Fibtex(Fibtex &&that) = delete;
	Fibtex &operator=(const Fibtex &) = delete;
	Fibtex &operator=(Fibtex &&that) = delete;

	~Fibtex() = default;

private:
	/**
	 * If the task scheduler only has 1 thread, there's no point of doing
	 * any spin waiting
	 */
	const bool m_ableToSpin;
	TaskScheduler *m_taskScheduler;

	/* We store the general lock, the queue lock, and the queue all in a single uintptr_t */
	std::atomic<uintptr_t> m_word;

	static constexpr uintptr_t kIsLockedBit = 1;
	static constexpr uintptr_t kIsQueueLockedBit = 2;
	static constexpr uintptr_t kQueueHeadMask = 3;

public:
	/**
	 * @brief Lock the Fibtex
	 *
	 * We use lower-case naming so users can use std::lock_guard and friends
	 *
	 * @param pinToCurrentThread    If true, this fiber won't be resumed on another thread (if locking puts it to sleep)
	 */
	// ReSharper disable once CppInconsistentNaming
	void lock(bool pinToCurrentThread = false) {
		// Fast path
		// No one has the lock and there are no waiters
		uintptr_t expected = 0;
		if (std::atomic_compare_exchange_weak_explicit(&m_word, &expected, kIsLockedBit, std::memory_order_acquire, std::memory_order_relaxed)) {
			// Lock acquired
			return;
		}

		LockSlow(pinToCurrentThread);
	}

	/**
	 * @brief Attempt to lock the Fibtex
	 *
	 * We use lower-case naming so users can use std::lock_guard and friends
	 *
	 * @return    True if succesful in locking. False otherwisekk
	 */
	bool try_lock() {
		// No one has the lock and there are no waiters
		uintptr_t expected = 0;
		if (std::atomic_compare_exchange_weak_explicit(&m_word, &expected, kIsLockedBit, std::memory_order_acquire, std::memory_order_relaxed)) {
			// Lock acquired
			return true;
		}

		// Try barging the lock
		uintptr_t currentWordValue = m_word.load();

		if ((currentWordValue & kIsLockedBit) == 0) {
			if (std::atomic_compare_exchange_weak(&m_word, &currentWordValue, currentWordValue | kIsLockedBit)) {
				// Success! We acquired the lock.
				return true;
			}
		}

		return false;
	}

	/**
	 * @brief Unlock the Fibtex
	 *
	 * We use lower-case naming so users can use std::lock_guard and friends
	 */
	// ReSharper disable once CppInconsistentNaming
	void unlock() {
		// Fast path
		// We have the lock and there are no waiters
		uintptr_t expected = kIsLockedBit;
		if (std::atomic_compare_exchange_weak_explicit(&m_word, &expected, 0, std::memory_order_release, std::memory_order_relaxed)) {
			// Lock released
			return;
		}

		UnlockSlow();
	}

private:
	void LockSlow(bool pinToCurrentThread);
	void UnlockSlow();
};

/**
 * A wrapper class that encapsulates pinToThread, and the various spin behaviors
 */
class LockWrapper {
public:
	/**
	 * @param mutex                 Fibtex to wrap around.
	 * @param pinToCurrentThread    If true, this fiber won't be resumed on another thread (if locking puts it to sleep)
	 */
	explicit LockWrapper(Fibtex &fibtex, bool pinToCurrentThread = false)
	        : m_fibtex(fibtex), m_pinToThread(pinToCurrentThread) {
	}
	/**
	 * Locks mutex
	 */
	void lock() {
		m_fibtex.lock(m_pinToThread);
	}
	/**
	 * Tries to lock mutex
	 *
	 * @return    If mutex successfully locked.
	 */
	bool try_lock() {
		return m_fibtex.try_lock();
	}
	/**
	 * Unlocks mutex.
	 */
	void unlock() const {
		m_fibtex.unlock();
	}

private:
	Fibtex &m_fibtex;
	bool m_pinToThread;
};

} // namespace ftl
