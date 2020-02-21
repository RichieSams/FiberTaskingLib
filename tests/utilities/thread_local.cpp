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

#include "ftl/thread_local.h"
#include "ftl/atomic_counter.h"
#include "ftl/task_scheduler.h"

#include <gtest/gtest.h>
#include <numeric>

namespace threadLocalTests {
ftl::ThreadLocal<size_t> &SingleInitSingleton(ftl::TaskScheduler *scheduler) {
	static ftl::ThreadLocal<size_t> counter(scheduler);

	return counter;
}

void SimpleInit(ftl::TaskScheduler *scheduler, void * /*arg*/) {
	*SingleInitSingleton(scheduler) += 1;
}

static std::atomic<size_t> g_sideEffectCount{0};
ftl::ThreadLocal<size_t> &SideEffectSingleton(ftl::TaskScheduler *scheduler) {
	static ftl::ThreadLocal<size_t> counter(scheduler, []() { return g_sideEffectCount++; });

	return counter;
}

void SideEffect(ftl::TaskScheduler *scheduler, void * /*arg*/) {
	*SideEffectSingleton(scheduler);
}

void MainTask(ftl::TaskScheduler *scheduler, void * /*arg*/) {
	// Single Init
	std::vector<ftl::Task> singleInitTask(scheduler->GetThreadCount(), ftl::Task{SimpleInit, nullptr});

	ftl::AtomicCounter ac(scheduler);
	scheduler->AddTasks(static_cast<uint>(singleInitTask.size()), singleInitTask.data(), &ac);
	scheduler->WaitForCounter(&ac, 0);

	auto singleInitVals = SingleInitSingleton(scheduler).GetAllValues();

	ASSERT_EQ(scheduler->GetThreadCount(), std::accumulate(singleInitVals.begin(), singleInitVals.end(), size_t{0}));

	// Side Effects
	std::vector<ftl::Task> sideEffectTask(10000, ftl::Task{SideEffect, nullptr});

	scheduler->AddTasks(static_cast<uint>(sideEffectTask.size()), sideEffectTask.data(), &ac);
	scheduler->WaitForCounter(&ac, 0);

	auto sideEffect = SideEffectSingleton(scheduler).GetAllValues();

	// The initializer will only fire once per thread, so there must be less than the thread count
	// in calls to it, but there should be at least one.
	ASSERT_LE(g_sideEffectCount, scheduler->GetThreadCount());
	ASSERT_GE(g_sideEffectCount, 1);
	// The count minus one should be the greatest value within the TLS.
	ASSERT_EQ(g_sideEffectCount - 1, *std::max_element(sideEffect.begin(), sideEffect.end()));
}
} // namespace threadLocalTests

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
TEST(FunctionalTests, ThreadLocal) {
	ftl::TaskScheduler taskScheduler;
	taskScheduler.Run(400, threadLocalTests::MainTask);
}
