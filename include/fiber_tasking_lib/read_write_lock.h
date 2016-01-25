/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2016
 */

#pragma once

#include "fiber_tasking_lib/config.h"




#ifdef FTL_OS_WINDOWS

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace FiberTaskingLib {

class ReadWriteLock {
public:
	ReadWriteLock()
		: m_lock(SRWLOCK_INIT) {
	}

private:
	SRWLOCK m_lock;

public:
	/**
	 * Checks for a write lock. If none exists, increments the reader count
	 */
	void LockRead() {
		AcquireSRWLockShared(&m_lock);
	}
	/**
	 * Decrements the reader count.
	 */
	void UnlockRead() {
		ReleaseSRWLockShared(&m_lock);
	}

	/**
	 * Acquires a write lock. Then waits until reader count == 0
	 */
	void LockWrite() {
		AcquireSRWLockExclusive(&m_lock);
	}
	/**
	 * Releases the write lock
	 */
	void UnlockWrite() {
		ReleaseSRWLockExclusive(&m_lock);
	}
};

} // End of namespace FiberTaskingLib

#elif defined(FTL_OS_LINUX) || defined(FTL_OS_OSX) || defined(FTL_OS_iOS)

#include <pthread.h>

namespace FiberTaskingLib {

class ReadWriteLock {
public:
	ReadWriteLock()
		: m_lock(PTHREAD_RWLOCK_INITIALIZER) {
	}
	~ReadWriteLock() {
		pthread_rwlock_destroy(&m_lock);
	}

private:
	pthread_rwlock_t m_lock;

public:
	/**
	* Checks for a write lock. If none exists, increments the reader count
	*/
	void LockRead() {
		pthread_rwlock_rdlock(&m_lock);
	}
	/**
	* Decrements the reader count.
	*/
	void UnlockRead() {
		pthread_rwlock_unlock(&m_lock);
	}

	/**
	* Acquires a write lock. Then waits until reader count == 0
	*/
	void LockWrite() {
		pthread_rwlock_wrlock(&m_lock);
	}
	/**
	* Releases the write lock
	*/
	void UnlockWrite() {
		pthread_rwlock_unlock(&m_lock);
	}
};

} // End of namespace FiberTaskingLib

#endif


