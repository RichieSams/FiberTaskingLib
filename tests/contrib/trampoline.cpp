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

#include "ftl/contrib/trampoline.h"

struct Foo {};

/*
	Showcases Trampoline functionality

	Requires C++17

	Features:
	 - automatic type safe interface and lambdas-as-tasks
	 - error if mismatched type/count of arguments (if a bit cryptic)
	 - no need to repeat types
	 - supports returning values from tasks

	Two usages showcased here: one with automatic memory management, and one with manual-ish memory management

	Downsides:
	 - C++17
	 - version A requires intrusive changes to AtomicCounter

	Missing for A:
	 - Multiple task addition support
	 - Task return value retrieval

	Glossary:
	 - Trampoline: wrapper around a lambda / function
	 - BoundTrampoline: Trampoline + arguments + possible return value
*/

void TrampolineMainTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	Foo f;
	int a = 3, b = 4, d = 6;

	ftl::AtomicCounter counter(taskScheduler);
	// Version A
	// Trampoline lifetime is automatically managed by the counter
	counter.addTask(
		ftl::Trampoline([](int& a, const int& b, const Foo& f, const int d) { a++; })
		.bind(a, b, f, d)
	);

	taskScheduler->WaitForCounter(&counter, 0);
	// values are correctly captured and const is respected
	std::cout << a << '\n';

	// Version B
	// Trampoline lifetime is managed by the user
	auto bt2 = ftl::Trampoline([](int& a, const int& b, const Foo& f, const int d) {return a+1; }).bind(a, b, f, d);
	// automatic conversion to ftl::Task
	taskScheduler->AddTask(*bt2, &counter);
	taskScheduler->WaitForCounter(&counter, 0);
	// showcases returning values
	std::cout << *bt2 << '\n';
}

TEST(ContribTests, Trampoline) {
	ftl::TaskScheduler taskScheduler;
	taskScheduler.Run(400, TrampolineMainTask);
}
