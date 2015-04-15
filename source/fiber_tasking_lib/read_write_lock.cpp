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

#include "fiber_tasking_lib/read_write_lock.h"

#include <cassert>


namespace FiberTaskingLib {


ReadWriteLock::ReadWriteLock() {
	m_readerCount.store(0u, std::memory_order_relaxed);
	InitializeCriticalSection(&m_writeLock);
}

ReadWriteLock::~ReadWriteLock() {
	DeleteCriticalSection(&m_writeLock);
}

void ReadWriteLock::LockRead() {
	EnterCriticalSection(&m_writeLock);
	m_readerCount.fetch_add(1);
	LeaveCriticalSection(&m_writeLock);
}

void ReadWriteLock::UnlockRead() {
	assert(m_readerCount.load(std::memory_order_relaxed) >= 0);

	m_readerCount.fetch_sub(1);
}

void ReadWriteLock::LockWrite() {
	// Prevent further reads
	EnterCriticalSection(&m_writeLock);

	// Wait until all the reads are finished
	while (m_readerCount.load(std::memory_order_relaxed) > 0) {
		// Yield timeslice
		SwitchToThread();
	}
}

void ReadWriteLock::UnlockWrite() {
	LeaveCriticalSection(&m_writeLock);
}

bool ReadWriteLock::TryUpgradeReadToWriteLock() {
	if (!TryEnterCriticalSection(&m_writeLock)) {
		return false;
	}

	// Wait until all but 1 reads are finished
	// We assume that the remaining reader is the one held by this thread
	// So if this function is called without first aquiring a read lock
	// the last reader will have undefined behavior
	while (m_readerCount.load(std::memory_order_relaxed) > 1) {
		// Yield timeslice
		SwitchToThread();
	}
}

} // End of namespace FiberTaskingLib
