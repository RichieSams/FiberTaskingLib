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

#include "ftl/task_counter.h"

namespace ftl {

/**
 * FullAtomicCounter is a wrapper over a C++11 atomic_unsigned
 * It implements TaskCounter and adds additional flexibility
 */
class FullAtomicCounter : public TaskCounter {
public:
	explicit FullAtomicCounter(TaskScheduler *taskScheduler, unsigned const initialValue = 0, size_t const fiberSlots = NUM_WAITING_FIBER_SLOTS)
	        : TaskCounter(taskScheduler, initialValue, fiberSlots) {
	}

	FullAtomicCounter(FullAtomicCounter const &) = delete;
	FullAtomicCounter(FullAtomicCounter &&) noexcept = delete;
	FullAtomicCounter &operator=(FullAtomicCounter const &) = delete;
	FullAtomicCounter &operator=(FullAtomicCounter &&) noexcept = delete;

public:
	/**
	 * A wrapper over std::atomic_unsigned::load()
	 *
	 * The load *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param memoryOrder    The memory order to use for the load
	 * @return               The current value of the counter
	 */
	unsigned Load(std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		unsigned ret = m_value.load(memoryOrder);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return ret;
	}
	/**
	 * A wrapper over std::atomic_unsigned::store()
	 *
	 * The store *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to load into the counter
	 * @param memoryOrder    The memory order to use for the store
	 */
	void Store(unsigned const x, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		m_value.store(x, memoryOrder);
		CheckWaitingFibers(x);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
	}
	/**
	 * A wrapper over std::atomic_unsigned::fetch_add()
	 *
	 * The fetch_add *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to add to the counter
	 * @param memoryOrder    The memory order to use for the fetch_add
	 * @return               The value of the counter before the addition
	 */
	unsigned FetchAdd(unsigned const x, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		const unsigned prev = m_value.fetch_add(x, memoryOrder);
		CheckWaitingFibers(prev + x);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return prev;
	}
	/**
	 * A wrapper over std::atomic_unsigned::fetch_sub()
	 *
	 * The fetch_sub *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param x              The value to subtract from the counter
	 * @param memoryOrder    The memory order to use for the fetch_sub
	 * @return               The value of the counter before the subtraction
	 */
	unsigned FetchSub(unsigned const x, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		const unsigned prev = m_value.fetch_sub(x, memoryOrder);
		CheckWaitingFibers(prev - x);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return prev;
	}

	/**
	 * A wrapper over std::atomic_unsigned::compare_exchange_strong()
	 *
	 * The compare_exchange_strong *will* be atomic, but this function as a whole is *not* atomic
	 *
	 * @param expectedValue    The value that is expected to be in the atomic counter
	 * @param newValue         The value that the atomic counter will be set to if comparison succeeds.
	 * @param memoryOrder      The memory order to use for the compare_exchange_strong
	 * @return                 If the compare_exchange_strong succeeded
	 */
	bool CompareExchange(unsigned expectedValue, unsigned const newValue, std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		bool const success = m_value.compare_exchange_strong(expectedValue, newValue, memoryOrder);
		if (success) {
			CheckWaitingFibers(newValue);
		}

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return success;
	}

protected:
	void Add(unsigned const x) override {
		FetchAdd(x, std::memory_order_seq_cst);
	}

	void Decrement() override {
		FetchSub(1, std::memory_order_seq_cst);
	}
};

/**
 * FullAtomicCounter is a wrapper over a C++11 atomic_unsigned
 * It implements TaskCounter and adds additional flexibility
 */
class AtomicFlag : public TaskCounter {
public:
	explicit AtomicFlag(TaskScheduler *taskScheduler, unsigned const initialValue = 0, size_t const fiberSlots = NUM_WAITING_FIBER_SLOTS)
	        : TaskCounter(taskScheduler, initialValue, fiberSlots) {
	}

	AtomicFlag(AtomicFlag const &) = delete;
	AtomicFlag(AtomicFlag &&) noexcept = delete;
	AtomicFlag &operator=(AtomicFlag const &) = delete;
	AtomicFlag &operator=(AtomicFlag &&) noexcept = delete;

public:
	bool Set(std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		return m_value.exchange(1U, memoryOrder) == 0;
	}

	bool Clear(std::memory_order const memoryOrder = std::memory_order_seq_cst) {
		m_lock.fetch_add(1U, std::memory_order_seq_cst);

		const bool success = m_value.exchange(0U, memoryOrder) == 1;
		if (!success) {
			m_lock.fetch_sub(1U, std::memory_order_seq_cst);
			return false;
		}
		CheckWaitingFibers(0U);

		m_lock.fetch_sub(1U, std::memory_order_seq_cst);
		return true;
	}
};

} // End of namespace ftl
