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

struct NumberSubset {
	uint64_t Start;
	uint64_t End;

	uint64_t Total;
};

void AddNumberSubset(ftl::TaskScheduler * /*scheduler*/, void *arg) {
	auto *subset = reinterpret_cast<NumberSubset *>(arg);

	subset->Total = 0;

	while (subset->Start != subset->End) {
		subset->Total += subset->Start;
		++subset->Start;
	}

	subset->Total += subset->End;
}

/**
 * Calculates the value of a triangle number by dividing the additions up into tasks
 *
 * A triangle number is defined as:
 *         Tn = 1 + 2 + 3 + ... + n
 *
 * The code is checked against the numerical solution which is:
 *         Tn = n * (n + 1) / 2
 *
 * TODO: Use gtest's 'Value Parameterized Tests' to test multiple triangle numbers
 */
TEST_CASE("Triangle Number", "[functional]") {
	ftl::TaskScheduler taskScheduler;
	REQUIRE(taskScheduler.Init() == 0);

	// Pick 100 random numbers to calculate
	uint64_t triangleNum = GENERATE(take(50, random(1000000ULL, 50000000ULL)));

	// Calculate the constants to test
	constexpr uint64_t numAdditionsPerTask = 10000ULL;
	const uint64_t numTasks = (triangleNum + numAdditionsPerTask - 1ULL) / numAdditionsPerTask;

	// Create the tasks
	auto *tasks = new ftl::Task[numTasks];
	// We have to declare this on the heap so other threads can access it
	auto *subsets = new NumberSubset[numTasks];
	uint64_t nextNumber = 1ULL;

	for (uint64_t i = 0ULL; i < numTasks; ++i) {
		NumberSubset *subset = &subsets[i];

		subset->Start = nextNumber;
		subset->End = nextNumber + numAdditionsPerTask - 1ULL;
		if (subset->End > triangleNum) {
			subset->End = triangleNum;
		}

		tasks[i] = {AddNumberSubset, subset};

		nextNumber = subset->End + 1;
	}

	// Schedule the tasks and wait for them to complete
	ftl::TaskCounter counter(&taskScheduler);
	taskScheduler.AddTasks(static_cast<unsigned>(numTasks), tasks, ftl::TaskPriority::Low, &counter);
	delete[] tasks;

	taskScheduler.WaitForCounter(&counter, 0);

	// Add the results
	uint64_t result = 0ULL;
	for (uint64_t i = 0; i < numTasks; ++i) {
		result += subsets[i].Total;
	}

	// Test
	REQUIRE(triangleNum * (triangleNum + 1ULL) / 2ULL == result);

	// Cleanup
	delete[] subsets;
}
