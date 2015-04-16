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
#include <list>
#include <memory>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


namespace FiberTaskingLib {

class TaskScheduler;
struct GlobalArgs;

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
		std::shared_ptr<AtomicCounter> Counter;
	};

	struct WaitingTask {
		WaitingTask() 
			: Fiber(nullptr), 
			  Counter(nullptr), 
			  Value(0) {
		}
		WaitingTask(void *fiber, AtomicCounter *counter, int value)
			: Fiber(fiber),
			  Counter(counter),
			  Value(value) {
		}

		void *Fiber;
		AtomicCounter *Counter;
		int Value;
	};

	moodycamel::ConcurrentQueue<TaskBundle> m_taskQueue;
	std::list<WaitingTask> m_waitingTasks;
	CRITICAL_SECTION m_waitingTaskLock;

	moodycamel::BlockingConcurrentQueue<void *> m_fiberPool;

	void *m_fiberSwitchingFiber;
	void *m_counterWaitingFiber;

	std::atomic_bool m_quit;

public:
	void Initialize(GlobalArgs *globalArgs);
	std::shared_ptr<AtomicCounter> AddTask(Task task);
	std::shared_ptr<AtomicCounter> AddTasks(uint numTasks, Task *tasks);

	void WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value);
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
