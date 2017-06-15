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

#pragma once

#include "fiber_tasking_lib/typedefs.h"

#include <atomic>
#include <vector>
#include <mutex>
#include <memory>


namespace FiberTaskingLib {

class TaskScheduler;

class AtomicCounter {

#define NUM_WAITING_FIBER_SLOTS 4

public:
	AtomicCounter(TaskScheduler *taskScheduler, uint value = 0) 
			: m_taskScheduler(taskScheduler),
			  m_value(value) {
		for (uint i = 0; i < NUM_WAITING_FIBER_SLOTS; ++i) {
			m_freeSlots[i].store(true);
		}
	}

private:
	TaskScheduler *m_taskScheduler;
	std::atomic_uint m_value;

	
	std::atomic_bool m_freeSlots[NUM_WAITING_FIBER_SLOTS];

	struct WaitingFiberBundle {
		WaitingFiberBundle()
			: InUse(true),
			  FiberIndex(0),
			  TargetValue(0), 
			  FiberStoredFlag(nullptr) {
		}

		std::atomic_bool InUse;
		std::size_t FiberIndex;
		uint TargetValue;
		std::atomic_bool *FiberStoredFlag;
	};
	WaitingFiberBundle m_waitingFibers[NUM_WAITING_FIBER_SLOTS];

public:
	uint Load(std::memory_order memoryOrder = std::memory_order_seq_cst) {
		return m_value.load(memoryOrder);
	}
	void Store(uint x, std::memory_order memoryOrder = std::memory_order_seq_cst) {
		m_value.store(x, memoryOrder);
		CheckWaitingFibers(x);
	}
	uint FetchAdd(uint x, std::memory_order memoryOrder = std::memory_order_seq_cst) {
		uint prev = m_value.fetch_add(x, memoryOrder);
		CheckWaitingFibers(prev + x);

		return prev;
	}
	uint FetchSub(uint x, std::memory_order memoryOrder = std::memory_order_seq_cst) {
		uint prev = m_value.fetch_sub(x, memoryOrder);
		CheckWaitingFibers(prev - x);

		return prev;
	}
	void AddFiberToWaitingList(std::size_t fiberIndex, uint targetValue, std::atomic_bool *fiberStoredFlag);

private:
	void CheckWaitingFibers(uint value);
};

} // End of namespace FiberTaskingLib
