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

	Features:
	 - automatic type safe interface and lambdas-as-tasks
	 - error if mismatched type/count of arguments (if a bit cryptic)
	 - no need to repeat types

	Glossary:
	 - Trampoline: wrapper around a lambda / function
	 - BoundTrampoline: Trampoline + arguments
*/

void TrampolineMainTask(ftl::TaskScheduler *taskScheduler, void *arg) {
	Foo f;
	int a = 3, b = 4, d = 6;

	ftl::AtomicCounter counter(taskScheduler);
	taskScheduler->AddTask(*ftl::make_trampoline([](int& a, const int& b, const Foo& f, const int d) { a++; }).bind(a, b, f, d), &counter);
	taskScheduler->WaitForCounter(&counter, 0);
	// values are correctly captured and const is respected
	std::cout << a << '\n';
}

TEST(ContribTests, Trampoline) {
	ftl::TaskScheduler taskScheduler;
	taskScheduler.Run(400, TrampolineMainTask);
}
