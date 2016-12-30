/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2016
 */

#include "fiber_tasking_lib/task_scheduler.h"

#include <gtest/gtest.h>


const uint kNumProducerTasks = 100u;
const uint kNumConsumerTasks = 10000u;


FTL_TASK_ENTRY_POINT(Consumer) {
	std::atomic_uint *globalCounter = reinterpret_cast<std::atomic_uint *>(arg);

	globalCounter->fetch_add(1);
}

FTL_TASK_ENTRY_POINT(Producer) {
	FiberTaskingLib::Task *tasks = new FiberTaskingLib::Task[kNumConsumerTasks];
	for (uint i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = { Consumer, arg };
	}

	std::shared_ptr<std::atomic_uint> counter = taskScheduler->AddTasks(kNumConsumerTasks, tasks);
	delete[] tasks;

	taskScheduler->WaitForCounter(counter, 0);
}


FTL_TASK_ENTRY_POINT(ProducerConsumerMainTask) {
	std::atomic_uint globalCounter(0u);

	FiberTaskingLib::Task tasks[kNumProducerTasks];
	for (uint i = 0; i < kNumProducerTasks; ++i) {
		tasks[i] = { Producer, &globalCounter };
	}

	std::shared_ptr<std::atomic_uint> counter = taskScheduler->AddTasks(kNumProducerTasks, tasks);
	taskScheduler->WaitForCounter(counter, 0);


	// Test to see that all tasks finished
	GTEST_ASSERT_EQ(kNumProducerTasks * kNumConsumerTasks, globalCounter.load());

	// Cleanup
}


/**
 * Tests that all scheduled tasks finish properly
 */
TEST(FunctionalTests, ProducerConsumer) {
	FiberTaskingLib::TaskScheduler taskScheduler;
	taskScheduler.Run(400, ProducerConsumerMainTask);
}
