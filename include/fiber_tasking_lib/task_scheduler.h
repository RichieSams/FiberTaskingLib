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
#include "fiber_tasking_lib/fiber.h"
#include "fiber_tasking_lib/task.h"
#include "fiber_tasking_lib/wait_free_queue.h"

#include <atomic>
#include <vector>
#include <climits>
#include <memory>


namespace FiberTaskingLib {

/**
 * A class that enables task-based multithreading.
 *
 * Underneath the covers, it uses fibers to allow cores to work on other tasks
 * when the current task is waiting on a synchronization atomic
 */
class TaskScheduler {
public:
	TaskScheduler();
	~TaskScheduler();

private:
	enum {
		FTL_INVALID_INDEX = UINT_MAX
	};

	std::size_t m_numThreads;
	std::vector<ThreadType> m_threads;	
	
	std::size_t m_fiberPoolSize;
	/* The backing storage for the fiber pool */
	Fiber *m_fibers;
	/**
	 * An array of atomics, which signify if a fiber is available to be used. The indices of m_waitingFibers
	 * correspond 1 to 1 with m_fibers. So, if m_freeFibers[i] == true, then m_fibers[i] can be used.
	 * Each atomic acts as a lock to ensure that threads do not try to use the same fiber at the same time
	 */
	std::atomic<bool> *m_freeFibers;
	/**
	  * An array of atomic, which signify if a fiber is waiting for a counter. The indices of m_waitingFibers
	  * correspond 1 to 1 with m_fibers. So, if m_waitingFibers[i] == true, then m_fibers[i] is waiting for a counter
	  */
	std::atomic<bool> *m_waitingFibers;

	/**
	 * Holds a Counter that is being waited on. Specifically, until Counter == TargetValue
	 */
	struct WaitingBundle {
		std::atomic_uint *Counter;
		uint TargetValue;
	};
	/**
	  * An array of WaitingBundles, which correspond 1 to 1 with m_waitingFibers. If m_waitingFiber[i] == true,
	  * m_waitingBundles[i] will contain the data for the waiting fiber in m_fibers[i].
	  */
	std::vector<WaitingBundle> m_waitingBundles;
	
	std::atomic_bool m_quit;
	
	enum class FiberDestination {
		None = 0,
		ToPool = 1,
		ToWaiting = 2,
	};

	/**
	* Holds a task that is ready to to be executed by the worker threads
	* Counter is the counter for the task(group). It will be decremented when the task completes
	*/
	struct TaskBundle {
		Task TaskToExecute;
		std::shared_ptr<std::atomic_uint> Counter;
	};

	struct ThreadLocalStorage {
		ThreadLocalStorage()
			: ThreadFiber(),
			  CurrentFiberIndex(FTL_INVALID_INDEX),
			  OldFiberIndex(FTL_INVALID_INDEX),
			  OldFiberDestination(FiberDestination::None),
			  TaskQueue(),
			  LastSuccessfulSteal(1) {
		}

		/**
		* Boost fibers require that fibers created from threads finish on the same thread where they started
		*
		* To accommodate this, we have save the initial fibers created in each thread, and immediately switch
		* out of them into the general fiber pool. Once the 'mainTask' has finished, we signal all the threads to
		* start quitting. When the receive the signal, they switch back to the ThreadFiber, allowing it to 
		* safely clean up.
		*/
		Fiber ThreadFiber;
		/* The index of the current fiber in m_fibers */
		std::size_t CurrentFiberIndex;
		/* The index of the previously executed fiber in m_fibers */
		std::size_t OldFiberIndex;
		/* Where OldFiber should be stored when we call CleanUpPoolAndWaiting() */
		FiberDestination OldFiberDestination;
		/* The queue of waiting tasks */
		WaitFreeQueue<TaskBundle> TaskQueue;
		/* The last queue that we successfully stole from. This is an offset index from the current thread index */
		std::size_t LastSuccessfulSteal;
	};
	/**
	 * c++ Thread Local Storage is, by definition, static/global. This poses some problems, such as multiple TaskScheduler
	 * instances. In addition, with Boost::Context, we have no way of telling the compiler to disable TLS optimizations, so we
	 * have to fake TLS anyhow. 
	 *
	 * During initialization of the TaskScheduler, we create one ThreadLocalStorage instance per thread. Threads index into
	 * their storage using m_tls[GetCurrentThreadIndex()]
	 */
	ThreadLocalStorage *m_tls;


public:
	/**
	 * Initializes the TaskScheduler and then starts executing 'mainTask'
	 *
	 * NOTE: Run will "block" until 'mainTask' returns. However, it doesn't block in the traditional sense; 'mainTask' is created as a Fiber.
	 * Therefore, the current thread will save it's current state, and then switch execution to the the 'mainTask' fiber. When 'mainTask'
	 * finishes, the thread will switch back to the saved state, and Run() will return.
	 *
	 * @param fiberPoolSize     The size of the fiber pool. The fiber pool is used to run new tasks when the current task is waiting on a counter
	 * @param mainTask          The main task to run
	 * @param mainTaskArg       The argument to pass to 'mainTask'
	 * @param threadPoolSize    The size of the thread pool to run. 0 corresponds to NumHarewareThreads()
	 */
	void Run(uint fiberPoolSize, TaskFunction mainTask, void *mainTaskArg = nullptr, uint threadPoolSize = 0);

	/**
	 * Adds a task to the internal queue.
	 *
	 * @param task    The task to queue
	 * @return        An atomic counter corresponding to this task. Initially it will equal 1. When the task completes, it will be decremented.
	 */
	std::shared_ptr<std::atomic_uint> AddTask(Task task);
	/**
	 * Adds a group of tasks to the internal queue
	 *
	 * @param numTasks    The number of tasks
	 * @param tasks       The tasks to queue
	 * @return            An atomic counter corresponding to the task group as a whole. Initially it will equal numTasks. When each task completes, it will be decremented.
	 */
	std::shared_ptr<std::atomic_uint> AddTasks(uint numTasks, Task *tasks);

	/**
	 * Yields execution to another task until counter == value
	 *
	 * @param counter    The counter to check
	 * @param value      The value to wait for
	 */
	void WaitForCounter(std::shared_ptr<std::atomic_uint> &counter, uint value);

private:
	/**
	 * Gets the 0-based index of the current thread
	 * This is useful for m_tls[GetCurrentThreadIndex()]
	 *
	 * @return    The index of the current thread
	 */
	std::size_t GetCurrentThreadIndex();
	/**
	 * Pops the next task off the queue into nextTask. If there are no tasks in the
	 * the queue, it will return false.
	 *
	 * @param nextTask    If the queue is not empty, will be filled with the next task
	 * @return            True: Successfully popped a task out of the queue
	 */
	bool GetNextTask(TaskBundle *nextTask);
	/**
	 * Gets the index of the next available fiber in the pool
	 *
	 * @return    The index of the next available fiber in the pool
	 */
	std::size_t GetNextFreeFiberIndex();
	/**
	 * If necessary, moves the old fiber to the fiber pool or the waiting list
	 * The old fiber is the last fiber to run on the thread before the current fiber
	 */
	void CleanUpOldFiber();

	/**
	 * The threadProc function for all worker threads
	 *
	 * @param arg    An instance of ThreadStartArgs
	 * @return       The return status of the thread
	 */
	static FTL_THREAD_FUNC_DECL ThreadStart(void *arg);
	/**
	* The fiberProc function that wraps the main fiber procedure given by the user
	*
	* @param arg    An instance of TaskScheduler
	*/
	static void MainFiberStart(void *arg);
	/**
	 * The fiberProc function for all fibers in the fiber pool
	 *
	 * @param arg    An instance of TaskScheduler
	 */
	static void FiberStart(void *arg);
};

} // End of namespace FiberTaskingLib
