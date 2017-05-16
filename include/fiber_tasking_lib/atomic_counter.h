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
public:
	AtomicCounter(TaskScheduler *taskScheduler, uint value = 0) 
		: m_taskScheduler(taskScheduler),
		  m_value(value) {
	}
	~AtomicCounter() {
		uint *foo = new uint(5);
		delete foo;
	}

private:
	TaskScheduler *m_taskScheduler;
	uint m_value;

	struct WaitingFiberBundle {
		WaitingFiberBundle(std::size_t fiberIndex, uint targetValue, bool storedFlag) 
			: FiberIndex(fiberIndex),
			  TargetValue(targetValue),
			  StoredFlag(std::make_unique<std::atomic_bool>(storedFlag)) {
		}
		WaitingFiberBundle(WaitingFiberBundle &&other) noexcept {
			std::swap(FiberIndex, other.FiberIndex);
			std::swap(TargetValue, other.TargetValue);
			StoredFlag = std::make_unique<std::atomic_bool>(other.StoredFlag->load(std::memory_order_relaxed));
		}
		WaitingFiberBundle &operator=(WaitingFiberBundle &&other) noexcept {
			std::swap(FiberIndex, other.FiberIndex);
			std::swap(TargetValue, other.TargetValue);
			StoredFlag = std::make_unique<std::atomic_bool>(other.StoredFlag->load(std::memory_order_relaxed));

			return *this;
		}

		std::size_t FiberIndex;
		uint TargetValue;
		std::unique_ptr<std::atomic_bool> StoredFlag;
	};
	std::vector <WaitingFiberBundle> m_waitingFibers;
	std::mutex m_lock;

public:
	uint Load() {
		std::lock_guard<std::mutex> guard(m_lock);

		return m_value;
	}
	void Store(uint x) {
		std::lock_guard<std::mutex> guard(m_lock);

		m_value = x;
		CheckWaitingFibers();
	}
	uint FetchAdd(uint x) {
		std::lock_guard<std::mutex> guard(m_lock);

		uint prev = m_value;
		m_value += x;
		CheckWaitingFibers();

		return prev;
	}
	uint FetchSub(uint x) {
		std::lock_guard<std::mutex> guard(m_lock);

		uint prev = m_value;
		m_value -= x;
		CheckWaitingFibers();

		return prev;
	}
	std::atomic_bool *AddFiberToWaitingList(std::size_t fiberIndex, uint targetValue);

private:
	void CheckWaitingFibers();
};

} // End of namespace FiberTaskingLib
