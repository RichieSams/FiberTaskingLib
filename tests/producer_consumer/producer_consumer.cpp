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

#include "ftl/atomic_counter.h"
#include "ftl/task_scheduler.h"

#include <gtest/gtest.h>


const uint kNumProducerTasks = 100u;
const uint kNumConsumerTasks = 10000u;


void Consumer(ftl::TaskScheduler *taskScheduler, void *arg) {
	std::atomic_uint *globalCounter = reinterpret_cast<std::atomic_uint *>(arg);

	globalCounter->fetch_add(1);
}

void Producer(ftl::TaskScheduler *taskScheduler, void *arg) {
	ftl::Task *tasks = new ftl::Task[kNumConsumerTasks];
	for (uint i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = { Consumer, arg };
	}

	ftl::AtomicCounter counter(taskScheduler);
	taskScheduler->AddTasks(kNumConsumerTasks, tasks, &counter);
	delete[] tasks;

	taskScheduler->WaitForCounter(&counter, 0);
}


void ProducerConsumerMainTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	std::atomic_uint globalCounter(0u);
	FTL_VALGRIND_HG_DISABLE_CHECKING(&globalCounter, sizeof(globalCounter));

	ftl::Task tasks[kNumProducerTasks];
	for (uint i = 0; i < kNumProducerTasks; ++i) {
		tasks[i] = { Producer, &globalCounter };
	}

	ftl::AtomicCounter counter(taskScheduler);
	taskScheduler->AddTasks(kNumProducerTasks, tasks, &counter);
	taskScheduler->WaitForCounter(&counter, 0);


	// Test to see that all tasks finished
	GTEST_ASSERT_EQ(kNumProducerTasks * kNumConsumerTasks, globalCounter.load());

	// Cleanup
}


/**
 * Tests that all scheduled tasks finish properly
 */
TEST(FunctionalTests, ProducerConsumer) {
	ftl::TaskScheduler taskScheduler;
	taskScheduler.Run(400, ProducerConsumerMainTask);
}
