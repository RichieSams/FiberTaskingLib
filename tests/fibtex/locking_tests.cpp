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

#include "ftl/fibtex.h"
#include "ftl/task_scheduler.h"

#include "gtest/gtest.h"

#include <atomic>

struct MutexData {
	MutexData(ftl::TaskScheduler *scheduler) : Lock(scheduler, 6), Counter(0) {
	}

	ftl::Fibtex Lock;
	unsigned Counter;
	std::vector<unsigned> Queue;
};

void LockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	std::lock_guard<ftl::Fibtex> lg(data->Lock);
	data->Queue.push_back(data->Counter++);
}

void SpinLockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::LockWrapper wrapper(data->Lock, ftl::FibtexLockBehavior::Spin);
	std::lock_guard<ftl::LockWrapper> lg(wrapper);
	data->Queue.push_back(data->Counter++);
}

void InfiniteSpinLockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	ftl::LockWrapper wrapper(data->Lock, ftl::FibtexLockBehavior::SpinInfinite);
	std::lock_guard<ftl::LockWrapper> lg(wrapper);
	data->Queue.push_back(data->Counter++);
}

void FutexMainTask(ftl::TaskScheduler *taskScheduler, void *) {
	MutexData md(taskScheduler);

	ftl::AtomicCounter c(taskScheduler);

	constexpr size_t iterations = 20000;
	for (size_t i = 0; i < iterations; ++i) {
		taskScheduler->AddTask(ftl::Task{LockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{LockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{SpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{SpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{InfiniteSpinLockGuardTest, &md}, &c);
		taskScheduler->AddTask(ftl::Task{InfiniteSpinLockGuardTest, &md}, &c);

		taskScheduler->WaitForCounter(&c, 0);
	}

	GTEST_ASSERT_EQ(md.Counter, 6 * iterations);
	GTEST_ASSERT_EQ(md.Queue.size(), 6 * iterations);
	for (unsigned i = 0; i < md.Counter; ++i) {
		GTEST_ASSERT_EQ(md.Queue[i], i);
	}
}

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FunctionalTests, LockingTests) {
	ftl::TaskScheduler taskScheduler;
	taskScheduler.Run(400, FutexMainTask, nullptr, 0, ftl::EmptyQueueBehavior::Yield);
}
