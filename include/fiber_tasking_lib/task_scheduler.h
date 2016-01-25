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

#include "fiber_tasking_lib/typedefs.h"
#include "fiber_tasking_lib/thread_abstraction.h"
#include "fiber_tasking_lib/fiber_abstraction.h"
#include "fiber_tasking_lib/task.h"

#include "concurrentqueue/blockingconcurrentqueue.h"

#include <atomic>
#include <vector>
#include <mutex>
#include <unordered_map>


namespace FiberTaskingLib {

typedef std::atomic_long AtomicCounter;


/**
 * A class that enables task-based multithreading.
 *
 * Underneath the covers, it uses fibers to allow cores to work on other tasks
 * when the current task is waiting on a synchronization atomic
 *
 * Note: Only one instance of this class should ever exist at a time.
 */
class TaskScheduler {
public:
	TaskScheduler();
	~TaskScheduler();

private:
	std::size_t m_numThreads;
	ThreadType *m_threads;
	std::atomic_uint m_numActiveWorkerThreads;

	/**
	 * Holds a task that is ready to to be executed by the worker threads
	 * Counter is the counter for the task(group). It will be decremented when the task completes
	 */
	struct TaskBundle {
		Task TaskToExecute;
		std::shared_ptr<AtomicCounter> Counter;
	};

	/**
	 * Holds a fiber that is waiting on a counter to be a certain value
	 */
	struct WaitingTask {
		WaitingTask()
			: Fiber(nullptr),
			  Counter(nullptr),
			  Value(0) {
		}
		WaitingTask(FiberType fiber, AtomicCounter *counter, int value)
			: Fiber(fiber),
			  Counter(counter),
			  Value(value) {
		}

		FiberType Fiber;
		AtomicCounter *Counter;
		int Value;
	};

	moodycamel::ConcurrentQueue<TaskBundle> m_taskQueue;
	std::vector<WaitingTask> m_waitingTasks;
	std::mutex m_waitingTaskLock;

	moodycamel::BlockingConcurrentQueue<FiberType> m_fiberPool;

	/**
	 * In order to put the current fiber on the waitingTasks list or the fiber pool, we have to
	 * switch to a different fiber. If we naively added ourselves to the list/fiber pool, then
	 * switch to the new fiber, another thread could pop from the list/pool and
	 * try to execute the current fiber before we have time to switch. This leads to stack corruption
	 * and/or general undefined behavior.
	 *
	 * So we use helper fibers to do the store/switch for us. However, we have to have a helper
	 * fiber for each thread. Otherwise, two threads could try to switch to the same helper fiber
	 * at the same time. Again, this leads to stack corruption and/or general undefined behavior.
	 */
	std::vector<FiberType> m_fiberSwitchingFibers;
	std::vector<FiberType> m_counterWaitingFibers;

	std::atomic_bool m_quit;

public:
	/**
	 * Creates the fiber pool and spawns worker threads for each (logical) CPU core. Each worker
	 * thread is affinity bound to a single core.
	 */
	bool Initialize(uint fiberPoolSize);

	/**
	 * Adds a task to the internal queue.
	 *
	 * @param task    The task to queue
	 * @return        An atomic counter corresponding to this task. Initially it will equal 1. When the task completes, it will be decremented.
	 */
	std::shared_ptr<AtomicCounter> AddTask(Task task);
	/**
	 * Adds a group of tasks to the internal queue
	 *
	 * @param numTasks    The number of tasks
	 * @param tasks       The tasks to queue
	 * @return            An atomic counter corresponding to the task group as a whole. Initially it will equal numTasks. When each task completes, it will be decremented.
	 */
	std::shared_ptr<AtomicCounter> AddTasks(uint numTasks, Task *tasks);

	/**
	 * Yields execution to another task until counter == value
	 *
	 * @param counter    The counter to check
	 * @param value      The value to wait for
	 */
	void WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value);
	/**
	 * Tells all worker threads to quit, then waits for all threads to complete.
	 * Any currently running task will finish before the worker thread returns.
	 *
	 * @return
	 */
	void Quit();

private:
	/**
	 * Pops the next task off the queue into nextTask. If there are no tasks in the
	 * the queue, it will return false.
	 *
	 * @param nextTask    If the queue is not empty, will be filled with the next task
	 * @return            True: Successfully popped a task out of the queue
	 */
	bool GetNextTask(TaskBundle *nextTask);
	/**
	 * Returns the current fiber back to the fiber pool and switches to fiberToSwitchTo
	 *
	 * Note: As noted above, we use helper fibers to do this switch for us.
	 *
	 * @param fiberToSwitchTo    The fiber to switch to
	 */
	void SwitchFibers(FiberType fiberToSwitchTo);

	/**
	 * The threadProc function for all worker threads
	 *
	 * @param arg    An instance of ThreadStartArgs
	 * @return       The return status of the thread
	 */
	static THREAD_FUNC_DECL ThreadStart(void *arg);
	/**
	 * The fiberProc function for all fibers in the fiber pool
	 *
	 * @param arg    An instance of TaskScheduler
	 */
	static FIBER_START_FUNCTION(FiberStart);
	/**
	 * The fiberProc function for the fiber switching helper fiber
	 *
	 * @param arg    An instance of TaskScheduler
	 */
	static FIBER_START_FUNCTION(FiberSwitchStart);
	/**
	 * The fiberProc function for the counter wait helper fiber
	 *
	 * @param arg    An instance of TaskScheduler
	 */
	static FIBER_START_FUNCTION(CounterWaitStart);
};

} // End of namespace FiberTaskingLib
