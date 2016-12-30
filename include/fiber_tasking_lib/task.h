/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
*
* This library was created as a proof of concept of the ideas presented by
* Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
*
* http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
*
* FiberTaskingLib is the legal property of Adrian Astley
* Copyright Adrian Astley 2015 - 2016
*/

#pragma once

namespace FiberTaskingLib {

class TaskScheduler;

typedef void(*TaskFunction)(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg);

/**
* Creates the correct function signature for a task entry point
*
* The function will have the following args:
*     TaskScheduler *g_taskScheduler,
*     void *arg
* where arg == Task::ArgData
*/
#define FTL_TASK_ENTRY_POINT(functionName) void functionName(FiberTaskingLib::TaskScheduler *taskScheduler, void *arg)

struct Task {
	TaskFunction Function;
	void *ArgData;
};

} // End of namespace FiberTaskingLib
