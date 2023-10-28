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

#include "ftl/parallel_for.h"

#include "catch2/catch_test_macros.hpp"

#include <atomic>
#include <vector>

TEST_CASE("Parallel For", "[utility]") {
	ftl::TaskScheduler taskScheduler;
	ftl::TaskSchedulerInitOptions options;
	options.Behavior = ftl::EmptyQueueBehavior::Yield;
	REQUIRE(taskScheduler.Init(options) == 0);

	constexpr unsigned size = 100;
	std::atomic<uint64_t> total(0);

	SECTION("Plain data array") {
		unsigned data[size];
		for (unsigned i = 0; i < size; ++i) {
			data[i] = i + 1;
		}

		ftl::ParallelFor(
		    &taskScheduler, data, size, 15, [&total](ftl::TaskScheduler *ts, unsigned *value) {
			    (void)ts;
			    total.fetch_add(*value);
		    },
		    ftl::TaskPriority::Normal
		);
	}
	SECTION("Iterable") {
		std::vector<unsigned> data(size);
		for (unsigned i = 0; i < size; ++i) {
			data[i] = i + 1;
		}

		ftl::ParallelFor(
		    &taskScheduler, data, 15, [&total](ftl::TaskScheduler *ts, unsigned *value) {
			    (void)ts;
			    total.fetch_add(*value);
		    },
		    ftl::TaskPriority::Normal
		);
	}

	constexpr uint64_t expectedValue = size * (size + 1) / 2;
	REQUIRE(total.load() == expectedValue);
}
