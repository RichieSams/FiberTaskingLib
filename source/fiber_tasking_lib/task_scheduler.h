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

#include "fiber_tasking_lib/typedefs.h"

#include "concurrentqueue/blockingconcurrentqueue.h"

#include <atomic>


namespace FiberTaskingLib {

class TaskScheduler;
class GlobalArgs;

typedef void (__stdcall *TaskFunction)(FiberTaskingLib::TaskScheduler *g_taskScheduler, void *arg);
#define TASK_FUNCTION(functionName) void __stdcall functionName(FiberTaskingLib::TaskScheduler *g_taskScheduler, void *arg)


typedef std::atomic_char32_t AtomicCounter;


struct Task {
	TaskFunction Function;
	void *ArgData;
};

#define FIBER_POOL_SIZE 150

class TaskScheduler {
public:
	TaskScheduler();
	~TaskScheduler();

private:
	DWORD m_numThreads;
	HANDLE *m_threads;

	struct TaskBundle {
		Task Task;
		AtomicCounter *Counter;
	};

	struct WaitingTask {
		void *Fiber;
		AtomicCounter *Counter;
		int Value;
	};

	moodycamel::ConcurrentQueue<TaskBundle> m_taskQueue;
	moodycamel::ConcurrentQueue<WaitingTask> m_waitingTasks;

	moodycamel::BlockingConcurrentQueue<void *> m_fiberPool;

	void *m_fiberSwitchingFiber;
	void *m_counterWaitingFiber;

	std::atomic_bool m_quit;

public:
	void Initialize(GlobalArgs *globalArgs);
	AtomicCounter *AddTask(Task task);
	AtomicCounter *AddTasks(uint numTasks, Task *tasks);

	void WaitForCounter(AtomicCounter *counter, int value);
	void Quit();

private:
	bool GetNextTask(TaskBundle *nextTask);
	void SwitchFibers(void *fiberToSwitchTo);

	static uint __stdcall ThreadStart(void *arg);
	static void __stdcall FiberStart(void *arg);
	static void __stdcall FiberSwitchStart(void *arg);
	static void __stdcall CounterWaitStart(void *arg);
};

} // End of namespace FiberTaskingLib
