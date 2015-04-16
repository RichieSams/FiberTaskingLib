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

#include <iostream>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

TASK_ENTRY_POINT(SecondLevel) {
	volatile int k = 0;
	for (uint j = 0; j < 10000; ++j) {
		k += 1;
	}

	std::cout << "second" << std::endl;
}

TASK_ENTRY_POINT(FirstLevel) {
	volatile int k = 0;
	for (uint j = 0; j < 1000000; ++j) {
		k += 1;
	}

	std::cout << "first" << std::endl;

	FiberTaskingLib::Task tasks[10];
	for (uint i = 0; i < 10; ++i) {
		tasks[i] = {SecondLevel, nullptr};
	}

	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = g_taskScheduler->AddTasks(10, tasks);
	g_taskScheduler->WaitForCounter(counter, 0);
}


int main() {
	FiberTaskingLib::GlobalArgs *globalArgs = new FiberTaskingLib::GlobalArgs();
	globalArgs->TaskScheduler.Initialize(globalArgs);	

	FiberTaskingLib::Task tasks[10];
	for (uint i = 0; i < 10; ++i) {
		tasks[i] = {FirstLevel, nullptr};
	}

	std::shared_ptr<FiberTaskingLib::AtomicCounter> counter = globalArgs->TaskScheduler.AddTasks(10, tasks);
	globalArgs->TaskScheduler.WaitForCounter(counter, 0);
	std::flush(std::cout);

	globalArgs->TaskScheduler.Quit();
	delete globalArgs;

	return 1;
}
