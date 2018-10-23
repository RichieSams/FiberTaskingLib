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

#include "ftl/task_scheduler.h"
#include "ftl/fibtex.h"

#include <gtest/gtest.h>
#include <atomic>


struct MutexData {
	ftl::Fibtex common_mutex;
	ftl::Fibtex second_mutex;
	std::atomic<std::size_t> counter;
};


void LockGuardTest(ftl::TaskScheduler& scheduler, void* arg) {
	auto* data = reinterpret_cast<MutexData*>(arg);

	ftl::LockGuard<ftl::Fibtex> lg(data->common_mutex);

	// Intentional non-atomic increment
	std::size_t value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
}


void SpinLockGuardTest(ftl::TaskScheduler& scheduler, void* arg) {
	auto* data = reinterpret_cast<MutexData*>(arg);

	ftl::SpinLockGuard<ftl::Fibtex> lg(data->common_mutex);

	// Intentional non-atomic increment
	std::size_t value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
}

void InfiniteSpinLockGuardTest(ftl::TaskScheduler& scheduler, void* arg) {
	auto* data = reinterpret_cast<MutexData*>(arg);

	ftl::InfiniteSpinLockGuard<ftl::Fibtex> lg(data->common_mutex);

	// Intentional non-atomic increment
	std::size_t value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
}


void UniqueLockGuardTest(ftl::TaskScheduler& scheduler, void* arg) {
	auto* data = reinterpret_cast<MutexData*>(arg);

	ftl::UniqueLock<ftl::Fibtex> lock(data->common_mutex, ftl::defer_lock);


	lock.lock();
	// Intentional non-atomic increment
	std::size_t value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
	lock.unlock();

	lock.lock_spin();
	// Intentional non-atomic increment
	value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
	lock.unlock();

	lock.lock_spin_infinite();
	// Intentional non-atomic increment
	value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
	lock.unlock();
}

void ScopeGuardTest(ftl::TaskScheduler& scheduler, void* arg) {
	auto* data = reinterpret_cast<MutexData*>(arg);

	auto lock = ftl::make_scoped_lock(false, data->common_mutex, data->second_mutex);

	// Intentional non-atomic increment
	std::size_t value = data->counter.load(std::memory_order_acquire);
	value += 1;
	data->counter.store(value, std::memory_order_release);
}

void FutexMainTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	MutexData md {
		ftl::Fibtex(taskScheduler),
		ftl::Fibtex(taskScheduler),
		0
	};
}
