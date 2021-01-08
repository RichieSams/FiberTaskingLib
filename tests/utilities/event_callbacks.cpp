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

TEST_CASE("Fiber Event Callbacks", "[utility]") {
	ftl::TaskSchedulerInitOptions options;
	options.ThreadPoolSize = 1;
	options.FiberPoolSize = 20;
	options.Behavior = ftl::EmptyQueueBehavior::Yield;

	struct TestCheckValues {
		std::atomic_int fiberEventNum;
	};
	TestCheckValues testValues{};
	testValues.fiberEventNum.store(0, std::memory_order_seq_cst);

	options.Callbacks.Context = &testValues;
	options.Callbacks.OnThreadsCreated = [](void *context, unsigned threadCount) {
		(void)context;
		REQUIRE(threadCount == 1);
	};
	options.Callbacks.OnFibersCreated = [](void *context, unsigned fiberCount) {
		(void)context;
		REQUIRE(fiberCount == 20);
	};
	options.Callbacks.OnWorkerThreadStarted = [](void *context, unsigned threadIndex) {
		(void)context;
		(void)threadIndex;
		FAIL("This should never be called, since we don't create worker threads");
	};
	options.Callbacks.OnWorkerThreadEnded = [](void *context, unsigned fiberIndex) {
		(void)context;
		(void)fiberIndex;
		FAIL("This should never be called, since we don't create worker threads");
	};
	options.Callbacks.OnFiberStateChanged = [](void *context, unsigned fiberIndex, ftl::FiberState newState) {
		REQUIRE(context != nullptr);
		TestCheckValues *values = static_cast<TestCheckValues *>(context);

		int eventNum = values->fiberEventNum.fetch_add(1, std::memory_order_seq_cst);
		INFO("Event number: " << eventNum);
		INFO("New state: " << static_cast<int>(newState));

		switch (eventNum) {
		case 0:
			// The first event should be the main thread being transitioned to be a fiber
			REQUIRE(fiberIndex == 0);
			REQUIRE(newState == ftl::FiberState::Attached);
			break;
		case 1:
			// This event should be the main fiber being unloaded, due to a wait
			REQUIRE(fiberIndex == 0);
			REQUIRE(newState == ftl::FiberState::Detached);
			break;
		case 2:
			// This event should be the next fiber in the pool being loaded to work on the task
			// NOTE: It's a bit of a hack to assume that we will get fiber index 1
			//       It's an implementation detail. However, for this test, I think that's ok
			REQUIRE(fiberIndex == 1);
			REQUIRE(newState == ftl::FiberState::Attached);
			break;
		case 3:
			// This event should be the worker fiber being unloaded, so we can go back to the main fiber
			REQUIRE(fiberIndex == 1);
			REQUIRE(newState == ftl::FiberState::Detached);
			break;
		case 4:
			// This event should be the main fiber being re-loaded after the wait
			REQUIRE(fiberIndex == 0);
			REQUIRE(newState == ftl::FiberState::Attached);
			break;
		case 5:
			// This event should be the main fiber being unloaded in the destructor
			REQUIRE(fiberIndex == 0);
			REQUIRE(newState == ftl::FiberState::Detached);
			break;
		default:
			FAIL("Event number is in a bad range");
		}
	};

	// Force a scope, so we can verify the behavior in the TaskScheduler destructor
	{
		ftl::TaskScheduler taskScheduler;
		REQUIRE(taskScheduler.Init(options) == 0);

		std::atomic_int taskRunCount(0);
		ftl::Task testTask{};
		testTask.ArgData = &taskRunCount;
		testTask.Function = [](ftl::TaskScheduler *ts, void *arg) {
			(void)ts;

			std::atomic_int *runCount = static_cast<std::atomic_int *>(arg);
			runCount->fetch_add(1, std::memory_order_seq_cst);
		};

		ftl::TaskCounter waitCounter(&taskScheduler);
		taskScheduler.AddTask(testTask, ftl::TaskPriority::Low, &waitCounter);

		taskScheduler.WaitForCounter(&waitCounter);

		REQUIRE(taskRunCount.load(std::memory_order_seq_cst) == 1);
	}
}

TEST_CASE("Thread Event Callbacks", "[utility]") {
	constexpr unsigned kThreadCount = 4;
	constexpr unsigned kFiberCount = 20;

	ftl::TaskSchedulerInitOptions options;
	options.ThreadPoolSize = kThreadCount;
	options.FiberPoolSize = kFiberCount;
	options.Behavior = ftl::EmptyQueueBehavior::Yield;

	struct TestCheckValues {
		std::atomic_int threadStarts[kThreadCount];
		std::atomic_int threadEnds[kThreadCount];
		std::atomic_int fiberAttaches[kThreadCount];
		std::atomic_int fiberDetaches[kThreadCount];
	};
	TestCheckValues testValues{};
	// Initialize
	for (unsigned i = 0; i < kThreadCount; ++i) {
		testValues.threadStarts[i].store(0);
		testValues.threadEnds[i].store(0);
		testValues.fiberAttaches[i].store(0);
		testValues.fiberDetaches[i].store(0);
	}

	options.Callbacks.Context = &testValues;
	options.Callbacks.OnThreadsCreated = [](void *context, unsigned threadCount) {
		(void)context;
		REQUIRE(threadCount == 4);
	};
	options.Callbacks.OnFibersCreated = [](void *context, unsigned fiberCount) {
		(void)context;
		REQUIRE(fiberCount == 20);
	};
	options.Callbacks.OnWorkerThreadStarted = [](void *context, unsigned threadIndex) {
		REQUIRE(context != nullptr);
		TestCheckValues *values = static_cast<TestCheckValues *>(context);

		// Check we only created kThreadCount threads
		REQUIRE(threadIndex < 4);

		// Check that we call start exactly once
		int prevValue = values->threadStarts[threadIndex].fetch_add(1, std::memory_order_seq_cst);
		REQUIRE(prevValue == 0);
	};
	options.Callbacks.OnWorkerThreadEnded = [](void *context, unsigned threadIndex) {
		REQUIRE(context != nullptr);
		TestCheckValues *values = static_cast<TestCheckValues *>(context);

		// Check we only created kThreadCount threads
		REQUIRE(threadIndex < 4);

		// Check that we call start exactly once
		int prevValue = values->threadEnds[threadIndex].fetch_add(1, std::memory_order_seq_cst);
		REQUIRE(prevValue == 0);
	};
	options.Callbacks.OnFiberStateChanged = [](void *context, unsigned fiberIndex, ftl::FiberState newState) {
		REQUIRE(context != nullptr);
		TestCheckValues *values = static_cast<TestCheckValues *>(context);

		// We never create any tasks / do waiting, so we should only use one fiber per thread
		REQUIRE(fiberIndex < 4);

		switch (newState) {
		case ftl::FiberState::Attached: {
			// Check that we call Attached exactly once
			int prevValue = values->fiberAttaches[fiberIndex].fetch_add(1, std::memory_order_seq_cst);
			REQUIRE(prevValue == 0);
			break;
		}
		case ftl::FiberState::Detached: {
			// Check that we call Detached exactly once
			int prevValue = values->fiberDetaches[fiberIndex].fetch_add(1, std::memory_order_seq_cst);
			REQUIRE(prevValue == 0);
			break;
		}
		default:
			FAIL("Invalid FiberState: " << static_cast<int>(newState));
		}
	};

	// Force a scope, so we can verify the behavior in the TaskScheduler destructor
	{
		ftl::TaskScheduler taskScheduler;
		REQUIRE(taskScheduler.Init(options) == 0);
	}

	// Verify that the main thread *didn't* call start/end
	// It's up to the user to do this, since ftl didn't create that thread
	REQUIRE(testValues.threadStarts[0].load(std::memory_order_seq_cst) == 0);
	REQUIRE(testValues.threadEnds[0].load(std::memory_order_seq_cst) == 0);

	// Check that all *worker* threads were started and ended
	for (unsigned i = 1; i < kThreadCount; ++i) {
		REQUIRE(testValues.threadStarts[i].load(std::memory_order_seq_cst) == 1);
		REQUIRE(testValues.threadEnds[i].load(std::memory_order_seq_cst) == 1);
	}

	// Check that we cleaned up the fibers that we used
	for (unsigned i = 0; i < kThreadCount; ++i) {
		REQUIRE(testValues.fiberAttaches[i].load(std::memory_order_seq_cst) == 1);
		REQUIRE(testValues.fiberDetaches[i].load(std::memory_order_seq_cst) == 1);
	}
}
