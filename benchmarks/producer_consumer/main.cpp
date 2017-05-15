/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2017
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

#include "fiber_tasking_lib/task_scheduler.h"


 // Constants
const uint kNumProducerTasks = 100u;
const uint kNumConsumerTasks = 1000u;
const uint kNumIterations = 50;

void Consumer(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg) {
	// No-Op
}

void Producer(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg) {
	FiberTaskingLib::Task *tasks = new FiberTaskingLib::Task[kNumConsumerTasks];
	for (uint i = 0; i < kNumConsumerTasks; ++i) {
		tasks[i] = { Consumer, arg };
	}

	std::atomic_uint counter;
	taskScheduler->AddTasks(kNumConsumerTasks, tasks, &counter);
	delete[] tasks;

	taskScheduler->WaitForCounter(&counter, 0);
}


void ProducerConsumerMainTask(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg) {
	FiberTaskingLib::Task tasks[kNumProducerTasks];
	for (uint i = 0; i < kNumProducerTasks; ++i) {
		tasks[i] = { Producer, nullptr };
	}

	auto start = std::chrono::high_resolution_clock::now();
	for (uint i = 0; i < kNumIterations; ++i) {
		std::atomic_uint counter;
		taskScheduler->AddTasks(kNumProducerTasks, tasks, &counter);

		taskScheduler->WaitForCounter(&counter, 0);
	}
	std::chrono::duration<float, std::milli> total = std::chrono::high_resolution_clock::now() - start;
	float average = total.count() / kNumIterations;

	printf("Total time: %.4fms\nNumber of iterations: %d\nAverage time: %.4fms\n", total.count(), kNumIterations, average);
}

int main(int argc, char **argv) {
	FiberTaskingLib::TaskScheduler taskScheduler;
	taskScheduler.Run(kNumProducerTasks + 20, ProducerConsumerMainTask);
}
