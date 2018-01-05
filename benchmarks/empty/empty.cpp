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

#include <nonius/nonius.hpp>


 // Constants
const uint kNumTasks = 65000;
const uint kNumIterations = 1;

void EmptyBenchmarkTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	// No-Op
}

void EmptyBenchmarkMainTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	auto& meter = *reinterpret_cast<nonius::chronometer*>(arg);

	ftl::Task *tasks = new ftl::Task[kNumTasks];
	for (uint i = 0; i < kNumTasks; ++i) {
		tasks[i] = {EmptyBenchmarkTask, nullptr};
	}

	meter.measure([=] {
		for (uint i = 0; i < kNumIterations; ++i) {
			ftl::AtomicCounter counter(taskScheduler);
			taskScheduler->AddTasks(kNumTasks, tasks, &counter);

			taskScheduler->WaitForCounter(&counter, 0);
		}
	});
	
	// Cleanup
	delete[] tasks;
}

NONIUS_BENCHMARK("Empty", [](nonius::chronometer meter) {
	ftl::TaskScheduler* taskScheduler = new ftl::TaskScheduler();
	taskScheduler->Run(20, EmptyBenchmarkMainTask, &meter);
	delete taskScheduler;
});
