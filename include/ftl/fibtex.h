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
#include <cassert>
#include <system_error>
#include <mutex>


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
	void lock(bool pinToThread = false) {
		while (true) {
			if (m_atomicCounter.CompareExchange(0, 1)) {
				return;
			}

			m_taskScheduler->WaitForCounter(&m_atomicCounter, 0, pinToThread);
		}
	}

	/**
	 * Lock mutex using an infinite spinlock. Does not spin if there is only one backing thread.
	 */
	void lock_spin_infinite(bool pinToThread = false) {
		// Don't spin if there is only one thread and spinning is pointless
		if (!m_ableToSpin) {
			lock(pinToThread);
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
	 * Lock mutex using a finite spinlock. Does not spin if there is only one backing thread.
	 *
	 * @param iterations    Amount of iterations to spin before yielding.
	 */
	void lock_spin(bool pinToThread = false, uint iterations = 1000) {
		// Don't spin if there is only one thread and spinning is pointless
		if (!m_ableToSpin) {
			lock(pinToThread);
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
		lock(pinToThread);
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
			printf("Error: Mutex was unlocked by another fiber or was double unlocked.\n");
			assert(false);
		}
	}

private:
	bool m_ableToSpin;
	ftl::TaskScheduler *m_taskScheduler;
	ftl::AtomicCounter m_atomicCounter;
};

/**
 * A lock guard with pinToThread support. Calls m.lock(pinToThread) and m.unlock().
 *
 * @tparam M    Mutex type
 */
template<class M>
class LockGuard {
public:
	/**
	 * Acquires the mutex with pinToThread settings.  Calls m.lock(pinToThread) and m.unlock().
	 *
	 * @param mutex          Mutex to acquire.
	 * @param pinToThread    If the fiber should resume on the same thread as it started on pre-lock.
	 */
	explicit LockGuard(M& mutex, bool pinToThread = false) : m_mutex(mutex) {
		m_mutex.lock(pinToThread);
	}
	/**
	 * Gets ownership of the mutex without locking it. Must be passed a locked mutex.
	 *
	 * @param m    Mutex to adopt ownership of.
	 * @param t    std::adopt_lock. Value unused.
	 */
	LockGuard(M& m, std::adopt_lock_t al) : m_mutex(m_mutex) {
		// Do not lock mutex
		(void) al;
	}
	LockGuard(const LockGuard&) = delete;
	LockGuard& operator=(const LockGuard&) = delete;

	~LockGuard() {
		m_mutex.unlock();
	}

private:
	M& m_mutex;
};

/**
 * A spin lock guard with pinToThread support. Calls m.lock_spin() and m.unlock().
 *
 * @tparam M    Mutex type
 */
template<class M>
class SpinLockGuard {
public:
	/**
	 * Acquires the mutex with pinToThread settings.  Calls m.lock_spin(pinToThread, iterations) and m.unlock().
	 *
	 * @param mutex          Mutex to acquire.
	 * @param pinToThread    If the fiber should resume on the same thread as it started on pre-lock.
	 * @param iterations     Amount of iterations to spin before yielding.
	 */
	explicit SpinLockGuard(M& mutex, bool pinToThread = false, uint iterations = 1000) : m_mutex(mutex) {
		m_mutex.lock_spin(pinToThread, iterations);
	}
	/**
	 * Gets ownership of the mutex without locking it. Must be passed a locked mutex.
	 *
	 * @param m    Mutex to adopt ownership of.
	 * @param t    std::adopt_lock. Value unused.
	 */
	SpinLockGuard(M& m, std::adopt_lock_t al) : m_mutex(m_mutex) {
		// Do not lock mutex
		(void) al;
	}
	SpinLockGuard(const SpinLockGuard&) = delete;
	SpinLockGuard& operator=(const SpinLockGuard&) = delete;

	~SpinLockGuard() {
		m_mutex.unlock();
	}

  private:
	M& m_mutex;
};

/**
 * A infinite spin lock guard with pinToThread support. Calls m.lock_spin_infinite() and m.unlock().
 *
 * @tparam M    Mutex type
 */
template<class M>
class InfiniteSpinLockGuard {
public:
	/**
	 * Acquires the mutex with pinToThread settings.  Calls m.spin_lock_infinite(pinToThread) and m.unlock().
	 *
	 * @param mutex          Mutex to acquire.
	 * @param pinToThread    If the fiber should resume on the same thread as it started on pre-lock.
	 */
	explicit InfiniteSpinLockGuard(M& mutex, bool pinToThread = false) : m_mutex(mutex) {
		m_mutex.spin_lock_infinite(pinToThread);
	}
	/**
	 * Gets ownership of the mutex without locking it. Must be passed a locked mutex.
	 *
	 * @param m    Mutex to adopt ownership of.
	 * @param t    std::adopt_lock. Value unused.
	 */
	InfiniteSpinLockGuard(M& m, std::adopt_lock_t al) : m_mutex(m_mutex) {
		// Do not lock mutex
		(void) al;
	}
	InfiniteSpinLockGuard(const InfiniteSpinLockGuard&) = delete;
	InfiniteSpinLockGuard& operator=(const InfiniteSpinLockGuard&) = delete;

	~InfiniteSpinLockGuard() {
		m_mutex.unlock();
	}

private:
	M& m_mutex;
};

template<class M>
class UniqueLock {
public:
	UniqueLock() noexcept : m_mutex(nullptr), m_hasMutex(false) {};
	UniqueLock(UniqueLock const& other) = delete;
	UniqueLock(UniqueLock&& other) noexcept {
		m_mutex = other.m_mutex;
		m_hasMutex = other.m_hasMutex;
		other.m_mutex = nullptr;
		other.m_hasMutex = false;
	};
	UniqueLock& operator=(UniqueLock const& other) = delete;
	UniqueLock& operator=(UniqueLock&& other) noexcept {
		m_mutex = other.m_mutex;
		m_hasMutex = other.m_hasMutex;
		other.m_mutex = nullptr;
		other.m_hasMutex = false;
		return *this;
	};

	UniqueLock(M& m, std::defer_lock_t dl) noexcept {
		// Don't actually lock mutex
		m_mutex = &m;
		m_hasMutex = false;
	}
	UniqueLock(M& m, std::try_to_lock_t tl, bool pinToThread = false) {
		// Only attempt to lock mutex
		m_mutex = &m;
		m_hasMutex = m_mutex->try_lock(pinToThread);
	}
	UniqueLock(M& m, std::adopt_lock_t al) {
		// Don't actually lock mutex
		m_mutex = &m;
		m_hasMutex = true;
	}

	void lock(bool pinToThread = false) {
		if(m_mutex) {
			if (!m_hasMutex) {
				m_mutex->lock(pinToThread);
				m_hasMutex = true;
			}
			else {
				throw std::system_error(EDEADLK, std::system_category());
			}
		}
		else {
			throw std::system_error(EPERM, std::system_category());
		}
	}

	bool try_lock(bool pinToThread = false) {
		if (m_mutex) {
			if (!m_hasMutex) {
				return m_hasMutex = m_mutex->try_lock(pinToThread);
			}
			throw std::system_error(EDEADLK, std::system_category());
		}
		throw std::system_error(EPERM, std::system_category());
	}

	void unlock() {
		if(m_mutex && m_hasMutex) {
			m_mutex->unlock();
			m_hasMutex = false;
		}
	}

	friend void swap(UniqueLock& lhs, UniqueLock& rhs) noexcept {
		using std::swap;

		swap(lhs.m_mutex, rhs.m_mutex);
		swap(lhs.m_hasMutex, rhs.m_hasMutex);
	}

	void swap(UniqueLock& that) noexcept {
		using std::swap;

		swap(*this, that);
	}

	M* release() noexcept {
		M* ret_val = m_mutex;
		m_mutex = nullptr;
		m_hasMutex = false;
		return ret_val;
	}

	M* mutex() const noexcept {
		return m_mutex;
	}

	bool owns_lock() const noexcept {
		return m_hasMutex;
	}

	explicit operator bool() const noexcept {
		return m_hasMutex;
	}

	~UniqueLock() {
		if (m_mutex) {
			m_mutex->unlock();
		}
	}
private:
	M* m_mutex;
	bool m_hasMutex;
};
}
