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
#include "fiber_tasking_lib/atomic_counter.h"


void EmptyBenchmarkTask(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg) {
	// No-Op
}

void EmptyBenchmarkMainTask(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg) {
	// Constants
	const uint kNumTasks = 65000;
	const uint kNumIterations = 50;
	
	// Create the tasks
	FiberTaskingLib::Task *tasks = new FiberTaskingLib::Task[kNumTasks];
	for (uint i = 0; i < kNumTasks; ++i) {
		tasks[i] = {EmptyBenchmarkTask, nullptr};
	}

	auto start = std::chrono::high_resolution_clock::now();
	for (uint i = 0; i < kNumIterations; ++i) {
		FiberTaskingLib::AtomicCounter counter(taskScheduler);
		taskScheduler->AddTasks(kNumTasks, tasks, &counter);

		taskScheduler->WaitForCounter(&counter, 0);
	}
	std::chrono::duration<float, std::milli> total = std::chrono::high_resolution_clock::now() - start;
	float average = total.count() / kNumIterations;

	printf("Total time: %.4fms\nNumber of iterations: %d\nAverage time: %.4fms\n", total.count(), kNumIterations, average);
}

int main(int argc, char **argv) {
	FiberTaskingLib::TaskScheduler taskScheduler;
	taskScheduler.Run(20, EmptyBenchmarkMainTask);
}
