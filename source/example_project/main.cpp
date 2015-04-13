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

TASK_FUNCTION(FirstLevel) {
	volatile int i = 0;
	for (uint j = 0; j < 10000; ++j) {
		i += 1;
	}
}


int main() {
	FiberTaskingLib::GlobalArgs *globalArgs = new FiberTaskingLib::GlobalArgs();
	globalArgs->TaskScheduler.Initialize(globalArgs);	

	FiberTaskingLib::Task tasks[10000];
	for (uint i = 0; i < 10000; ++i) {
		tasks[i] = {FirstLevel, nullptr};
	}

	FiberTaskingLib::AtomicCounter *counter = globalArgs->TaskScheduler.AddTasks(10000, tasks);
	globalArgs->TaskScheduler.WaitForCounter(counter, 0);
	std::flush(std::cout);

	globalArgs->TaskScheduler.Quit();
	delete globalArgs;

	return 1;
}
