/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
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
	FiberTaskingLib::Task tasks[kNumConsumerTasks];
	for (uint i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = {Consumer, arg};
	}

	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = g_taskScheduler->AddTasks(kNumConsumerTasks, tasks);
	g_taskScheduler->WaitForCounter(counter, 0);
}




/**
 * Tests that all scheduled tasks finish properly
 */
TEST(FiberTaskingLib, ProducerConsumer) {
	FiberTaskingLib::GlobalArgs *globalArgs = new FiberTaskingLib::GlobalArgs();
	globalArgs->TaskScheduler.Initialize(110, globalArgs);
	globalArgs->Allocator.init(&globalArgs->Heap, 1234);

	FiberTaskingLib::AtomicCounter *globalCounter = new FiberTaskingLib::AtomicCounter();
	globalCounter->store(0);

	FiberTaskingLib::Task tasks[kNumProducerTasks];
	for (uint i = 0; i < kNumProducerTasks; ++i) {
		tasks[i] = {Producer, globalCounter};
	}

	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = globalArgs->TaskScheduler.AddTasks(kNumProducerTasks, tasks);
	globalArgs->TaskScheduler.WaitForCounter(counter, 0);


	// Test to see that all tasks finished
	GTEST_ASSERT_EQ(kNumProducerTasks * kNumConsumerTasks, globalCounter->load());


	// Cleanup
	globalArgs->TaskScheduler.Quit();
	globalArgs->Allocator.destroy();
	delete globalArgs;
}
