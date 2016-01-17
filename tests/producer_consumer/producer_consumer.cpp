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
#include "fiber_tasking_lib/global_args.h"

#include <gtest/gtest.h>


const uint kNumProducerTasks = 100u;
const uint kNumConsumerTasks = 10000u;


TASK_ENTRY_POINT(Consumer) {
	FiberTaskingLib::AtomicCounter *globalCounter = (FiberTaskingLib::AtomicCounter *)arg;

	globalCounter->fetch_add(1);
}

TASK_ENTRY_POINT(Producer) {
	FiberTaskingLib::Task *tasks = new FiberTaskingLib::Task[kNumConsumerTasks];
	for (uint i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = {Consumer, arg};
	}

	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = g_taskScheduler->AddTasks(kNumConsumerTasks, tasks);
	delete[] tasks;

	g_taskScheduler->WaitForCounter(counter, 0);
}




/**
 * Tests that all scheduled tasks finish properly
 */
TEST(FunctionalTests, ProducerConsumer) {
	FiberTaskingLib::GlobalArgs *globalArgs = new FiberTaskingLib::GlobalArgs();
	globalArgs->g_taskScheduler.Initialize(110, globalArgs);
	globalArgs->g_allocator.init(&globalArgs->g_heap, 1234);

	FiberTaskingLib::AtomicCounter *globalCounter = new FiberTaskingLib::AtomicCounter();
	globalCounter->store(0);

	FiberTaskingLib::Task tasks[kNumProducerTasks];
	for (uint i = 0; i < kNumProducerTasks; ++i) {
		tasks[i] = {Producer, globalCounter};
	}

	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = globalArgs->g_taskScheduler.AddTasks(kNumProducerTasks, tasks);
	globalArgs->g_taskScheduler.WaitForCounter(counter, 0);


	// Test to see that all tasks finished
	GTEST_ASSERT_EQ(kNumProducerTasks * kNumConsumerTasks, globalCounter->load());


	// Cleanup
	globalArgs->g_taskScheduler.Quit();
	globalArgs->g_allocator.destroy();
	delete globalArgs;
}
