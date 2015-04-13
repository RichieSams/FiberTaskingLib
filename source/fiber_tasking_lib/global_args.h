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

#pragma once

#include "fiber_tasking_lib/task_scheduler.h"
#include "fiber_tasking_lib/linear_allocator.h"


namespace FiberTaskingLib {

struct GlobalArgs {
	GlobalArgs()
		: TaskScheduler(),
		  Allocator(2097152) {
	}

	TaskScheduler TaskScheduler;
	LinearAllocator Allocator;
};

} // End of namespace FiberTaskingLib
