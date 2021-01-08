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
#include "ftl/task_counter.h"
#include "ftl/task_scheduler.h"

#include "catch2/catch.hpp"

#include <numeric>

void SimpleInit(ftl::TaskScheduler *scheduler, void *arg) {
	(void)scheduler;
	ftl::ThreadLocal<size_t> *counter = static_cast<ftl::ThreadLocal<size_t> *>(arg);

	// Double dereference
	// First to get from ThreadLocal pointer to instance
	// Second to get from ThreadLocal to our thread-specific instance of size_t
	(*(*counter)) += 1;
}

void SideEffect(ftl::TaskScheduler *scheduler, void *arg) {
	(void)scheduler;
	ftl::ThreadLocal<size_t> *counter = static_cast<ftl::ThreadLocal<size_t> *>(arg);

	// Double dereference
	// First to get from ThreadLocal pointer to instance
	// Second to get from ThreadLocal to our thread-specific instance of size_t
	(*(*counter));
}

static std::atomic<size_t> g_sideEffectCount{0};

TEST_CASE("Thread Local", "[utility]") {
	ftl::TaskScheduler taskScheduler;
	REQUIRE(taskScheduler.Init() == 0);

	// Single Init
	ftl::ThreadLocal<size_t> simpleCounter(&taskScheduler);

	std::vector<ftl::Task> singleInitTask(taskScheduler.GetThreadCount(), ftl::Task{SimpleInit, &simpleCounter});

	ftl::TaskCounter ac(&taskScheduler);
	taskScheduler.AddTasks(static_cast<unsigned>(singleInitTask.size()), singleInitTask.data(), ftl::TaskPriority::Low, &ac);
	taskScheduler.WaitForCounter(&ac);

	auto singleInitVals = simpleCounter.GetAllValues();
	REQUIRE(taskScheduler.GetThreadCount() == std::accumulate(singleInitVals.begin(), singleInitVals.end(), size_t{0}));

	// Side Effects
	ftl::ThreadLocal<size_t> sideEffectCounter(&taskScheduler, []() { return g_sideEffectCount++; });

	std::vector<ftl::Task> sideEffectTask(10000, ftl::Task{SideEffect, &sideEffectCounter});

	taskScheduler.AddTasks(static_cast<unsigned>(sideEffectTask.size()), sideEffectTask.data(), ftl::TaskPriority::Low, &ac);
	taskScheduler.WaitForCounter(&ac);

	auto sideEffect = sideEffectCounter.GetAllValues();

	// The initializer will only fire once per thread, so there must be less than the thread count
	// in calls to it, but there should be at least one.
	REQUIRE(g_sideEffectCount <= taskScheduler.GetThreadCount());
	REQUIRE(g_sideEffectCount >= 1);
	// The count minus one should be the greatest value within the TLS.
	REQUIRE(g_sideEffectCount - 1 == *std::max_element(sideEffect.begin(), sideEffect.end()));
}
