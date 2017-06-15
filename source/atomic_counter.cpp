/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2017
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

#include "fiber_tasking_lib/atomic_counter.h"

#include "fiber_tasking_lib/task_scheduler.h"


namespace FiberTaskingLib {

void AtomicCounter::AddFiberToWaitingList(std::size_t fiberIndex, uint targetValue, std::atomic_bool *fiberStoredFlag) {
	for (uint i = 0; i < NUM_WAITING_FIBER_SLOTS; ++i) {
		bool expected = true;
		if (!std::atomic_compare_exchange_strong_explicit(&m_freeSlots[i], &expected, false, std::memory_order_seq_cst, std::memory_order_relaxed)) {
			// Failed the race
			continue;
		}

		// We found a free slot
		m_waitingFibers[i].FiberIndex = fiberIndex;
		m_waitingFibers[i].TargetValue = targetValue;
		m_waitingFibers[i].FiberStoredFlag = fiberStoredFlag;
		m_waitingFibers[i].InUse.store(false, std::memory_order_release);
		
		// Events are now being tracked


		// Now we do a check of the waiting fiber, to see if we reached the target value while we were storing everything
		if (m_waitingFibers[i].InUse.load(std::memory_order_relaxed)) {
			return;
		}

		if (m_waitingFibers[i].TargetValue == m_value.load(std::memory_order_relaxed)) {
			expected = false;
			if (!std::atomic_compare_exchange_strong_explicit(&m_waitingFibers[i].InUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				// Failed the race
				continue;
			}
			m_taskScheduler->AddReadyFiber(m_waitingFibers[i].FiberIndex, m_waitingFibers[i].FiberStoredFlag);
			m_freeSlots[i].store(true, std::memory_order_release);
		}

		return;
	}


	// BARF. We ran out of slots
	printf("All the waiting fiber slots are full. Not able to add another wait.\nIncrease the value of NUM_WAITING_FIBER_SLOTS or modify your algorithm to use less waits on the same counter");
	assert(false);
}

void AtomicCounter::CheckWaitingFibers(uint value) {
	for (uint i = 0; i < NUM_WAITING_FIBER_SLOTS; ++i) {
		if (m_freeSlots[i].load(std::memory_order_acquire)) {
			continue;
		}
		if (m_waitingFibers[i].InUse.load(std::memory_order_relaxed)) {
			continue;
		}

		if (m_waitingFibers[i].TargetValue == value) {
			bool expected = false;
			if (!std::atomic_compare_exchange_strong_explicit(&m_waitingFibers[i].InUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				// Failed the race
				continue;
			}
			m_taskScheduler->AddReadyFiber(m_waitingFibers[i].FiberIndex, m_waitingFibers[i].FiberStoredFlag);
			m_freeSlots[i].store(true, std::memory_order_release);
		}
	}
}


} // End of namespace FiberTaskingLib
