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
#include "ftl/wait_group.h"

#include "catch2/catch_test_macros.hpp"

struct MutexData {
	explicit MutexData(ftl::TaskScheduler *scheduler)
	        : Lock(scheduler) {
	}

	ftl::Fibtex Lock;
	unsigned Counter = 0;
	std::vector<unsigned> Queue;
};

void LockGuardTest(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *data = reinterpret_cast<MutexData *>(arg);

	std::lock_guard<ftl::Fibtex> lg(data->Lock);
	data->Queue.push_back(data->Counter++);
}

TEST_CASE("Fibtex Locking Tests", "[utility]") {
	ftl::TaskScheduler taskScheduler;
	ftl::TaskSchedulerInitOptions options;
	options.Behavior = ftl::EmptyQueueBehavior::Yield;
	REQUIRE(taskScheduler.Init(options) == 0);

	MutexData md(&taskScheduler);

	ftl::WaitGroup wg(&taskScheduler);

	constexpr size_t iterations = 20000;
	for (size_t i = 0; i < iterations; ++i) {
		taskScheduler.AddTask(ftl::Task{ LockGuardTest, &md }, ftl::TaskPriority::Normal, &wg);
		taskScheduler.AddTask(ftl::Task{ LockGuardTest, &md }, ftl::TaskPriority::Normal, &wg);

		wg.Wait();
	}

	REQUIRE(md.Counter == 2 * iterations);
	REQUIRE(md.Queue.size() == 2 * iterations);
	for (unsigned i = 0; i < md.Counter; ++i) {
		REQUIRE(md.Queue[i] == i);
	}
}
