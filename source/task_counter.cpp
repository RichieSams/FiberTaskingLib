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

#include "ftl/task_counter.h"

#include "ftl/ftl_valgrind.h"
#include "ftl/task_scheduler.h"

namespace ftl {

TaskCounter::TaskCounter(TaskScheduler *const taskScheduler, unsigned const initialValue, size_t const fiberSlots)
        : m_taskScheduler(taskScheduler), m_value(initialValue), m_lock(0), m_fiberSlots(fiberSlots) {
	m_freeSlots = new std::atomic<bool>[fiberSlots];
	m_waitingFibers = new WaitingFiberBundle[fiberSlots];

	FTL_VALGRIND_HG_DISABLE_CHECKING(&m_value, sizeof(m_value));
	FTL_VALGRIND_HG_DISABLE_CHECKING(&m_lock, sizeof(m_lock));
	FTL_VALGRIND_HG_DISABLE_CHECKING(m_freeSlots, sizeof(m_freeSlots[0]) * fiberSlots);
	FTL_VALGRIND_HG_DISABLE_CHECKING(m_waitingFibers, sizeof(m_waitingFibers[0]) * fiberSlots);

	for (unsigned i = 0; i < fiberSlots; ++i) {
		m_freeSlots[i].store(true);

		// We initialize InUse to true to prevent CheckWaitingFibers() from checking garbage
		// data when we are adding a new fiber to the wait list in AddFiberToWaitingList()
		// For this same reason, when we set a slot to be free (ie. m_freeSlots[i] = true), we
		// keep InUse == true
		m_waitingFibers[i].InUse.store(true);
	}
}

TaskCounter::~TaskCounter() {
	// We can't destroy the counter until all other threads have left the member functions
	while (m_lock.load(std::memory_order_relaxed) > 0) {
		std::this_thread::yield();
	}

	delete[] m_freeSlots;
	delete[] m_waitingFibers;
}

TaskCounter::WaitingFiberBundle::WaitingFiberBundle()
        : InUse(true), PinnedThreadIndex(std::numeric_limits<size_t>::max()) {
	FTL_VALGRIND_HG_DISABLE_CHECKING(&InUse, sizeof(InUse));
}

void TaskCounter::Add(unsigned x) {
	m_value.fetch_add(x, std::memory_order_seq_cst);
}

void TaskCounter::Decrement() {
	m_lock.fetch_add(1U, std::memory_order_seq_cst);

	const unsigned prev = m_value.fetch_sub(1U, std::memory_order_seq_cst);
	const unsigned newValue = prev - 1;

	// TaskCounters are only allowed to wait on 0, so we only need to check when newValue would be zero
	if (newValue == 0) {
		CheckWaitingFibers(newValue);
	}

	m_lock.fetch_sub(1U, std::memory_order_seq_cst);
}

bool TaskCounter::AddFiberToWaitingList(void *fiberBundle, unsigned targetValue, size_t const pinnedThreadIndex) {
	for (size_t i = 0; i < m_fiberSlots; ++i) {
		bool expected = true;
		// Try to acquire the slot
		if (!std::atomic_compare_exchange_strong_explicit(&m_freeSlots[i], &expected, false, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			// Failed the race or the slot was already full
			continue;
		}

		// We found a free slot
		m_waitingFibers[i].FiberBundle = fiberBundle;
		m_waitingFibers[i].TargetValue = targetValue;
		m_waitingFibers[i].PinnedThreadIndex = pinnedThreadIndex;
		// We have to use memory_order_seq_cst here instead of memory_order_acquire to prevent
		// later loads from being re-ordered before this store
		m_waitingFibers[i].InUse.store(false, std::memory_order_seq_cst);

		// Events are now being tracked

		// Now we do a check of the waiting fiber, to see if we reached the target value while we were storing
		// everything
		unsigned const value = m_value.load(std::memory_order_relaxed);
		if (m_waitingFibers[i].InUse.load(std::memory_order_acquire)) {
			return false;
		}

		if (m_waitingFibers[i].TargetValue == value) {
			expected = false;
			// Try to acquire InUse
			if (!std::atomic_compare_exchange_strong_explicit(&m_waitingFibers[i].InUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				// Failed the race. Another thread got to it first.
				return false;
			}
			// Signal that the slot is now free
			// Leave IneUse == true
			m_freeSlots[i].store(true, std::memory_order_release);

			return true;
		}

		return false;
	}

	// BARF. We ran out of slots
	// ReSharper disable once CppUnreachableCode
	FTL_ASSERT("All the waiting fiber slots are full. Not able to add another wait.\n"
	           "Increase the value of fiberSlots in the constructor or modify your algorithm to use less waits on the same counter",
	           false);
	return false;
}

void TaskCounter::CheckWaitingFibers(unsigned const value) {
	for (size_t i = 0; i < m_fiberSlots; ++i) {
		// Check if the slot is full
		if (m_freeSlots[i].load(std::memory_order_acquire)) {
			continue;
		}
		// Check if the slot is being modified by another thread
		if (m_waitingFibers[i].InUse.load(std::memory_order_acquire)) {
			continue;
		}

		// Do the actual value check
		if (m_waitingFibers[i].TargetValue == value) {
			bool expected = false;
			// Try to acquire InUse
			if (!std::atomic_compare_exchange_strong_explicit(&m_waitingFibers[i].InUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				// Failed the race. Another thread got to it first
				continue;
			}

			m_taskScheduler->AddReadyFiber(m_waitingFibers[i].PinnedThreadIndex, reinterpret_cast<TaskScheduler::ReadyFiberBundle *>(m_waitingFibers[i].FiberBundle));
			// Signal that the slot is free
			// Leave InUse == true
			m_freeSlots[i].store(true, std::memory_order_release);
		}
	}
}

} // End of namespace ftl
