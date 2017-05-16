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

std::atomic_bool *AtomicCounter::AddFiberToWaitingList(std::size_t fiberIndex, uint targetValue) {
	std::lock_guard<std::mutex> lockGuard(m_lock);
	
	if (m_value == targetValue) return nullptr; // if the counter is ready, do not enqueue fiber, early out

	m_waitingFibers.emplace_back(fiberIndex, targetValue, false);
	return m_waitingFibers.back().StoredFlag.get();
}

void AtomicCounter::CheckWaitingFibers() {
	for (auto iter = m_waitingFibers.begin(); iter != m_waitingFibers.end(); ) {
		if (m_value == iter->TargetValue) {
			while (!iter->StoredFlag->load(std::memory_order_acquire)) {
				// spin wait for fiber to be ready
			}
			m_taskScheduler->AddReadyFiber(iter->FiberIndex);
			iter = m_waitingFibers.erase(iter);
		} else {
			++iter;
		}
	}
}


} // End of namespace FiberTaskingLib
