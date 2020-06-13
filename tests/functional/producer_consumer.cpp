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
#include "ftl/task_scheduler.h"

#include "catch2/catch.hpp"

#include <array>

constexpr static unsigned kNumProducerTasks = 100U;
constexpr static unsigned kNumConsumerTasks = 10000U;

void Consumer(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *globalCounter = reinterpret_cast<std::atomic<unsigned> *>(arg);

	globalCounter->fetch_add(1);
}

void Producer(ftl::TaskScheduler *taskScheduler, void *arg) {
	auto *tasks = new ftl::Task[kNumConsumerTasks];
	for (unsigned i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = {Consumer, arg};
	}

	ftl::TaskCounter counter(taskScheduler);
	taskScheduler->AddTasks(kNumConsumerTasks, tasks, ftl::TaskPriority::Low, &counter);
	delete[] tasks;

	taskScheduler->WaitForCounter(&counter, 0);
}

/**
 * Tests that all scheduled tasks finish properly
 */
TEST_CASE("Producer Consumer", "[functional]") {
	ftl::TaskScheduler taskScheduler;
	REQUIRE(taskScheduler.Init() == 0);

	std::atomic<unsigned> globalCounter(0U);
	FTL_VALGRIND_HG_DISABLE_CHECKING(&globalCounter, sizeof(globalCounter));

	std::array<ftl::Task, kNumProducerTasks> tasks{};
	for (auto &&task : tasks) {
		task = {Producer, &globalCounter};
	}

	ftl::TaskCounter counter(&taskScheduler);
	taskScheduler.AddTasks(kNumProducerTasks, tasks.data(), ftl::TaskPriority::Low, &counter);
	taskScheduler.WaitForCounter(&counter, 0);

	// Test to see that all tasks finished
	REQUIRE(kNumProducerTasks * kNumConsumerTasks == globalCounter.load());
}
