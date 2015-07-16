/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
 */

#pragma once

#include "fiber_tasking_lib/typedefs.h"

#include <atomic>
#include <mutex>


namespace FiberTaskingLib {

class ReadWriteLock {
public:
	ReadWriteLock();
	~ReadWriteLock();

private:
	std::mutex m_writeLock;

	std::atomic_uint m_readerCount;

public:
	/**
	 * Checks for a write lock. If none exists, increments the reader count
	 */
	void LockRead();
	/**
	 * Decrements the reader count.
	 */
	void UnlockRead();

	/**
	 * Acquires a write lock. Then waits until reader count == 0
	 */
	void LockWrite();
	/**
	 * Releases the write lock
	 */
	void UnlockWrite();

	/**
	 * Tries to acquire a write lock. If it fails, returns false
	 * If successful, it waits until reader count == 1, assuming the
	 * last read lock is its own. If this is not the case, the last
	 * reader will have undefined behavior.
	 *
	 * NOTE: This function does not release the read lock. It must be released
	 *       as normal with UnlockRead()
	 *
	 * @return    Whether the upgrade was successful
	 */
	bool TryUpgradeReadToWriteLock();
};

} // End of namespace FiberTaskingLib
