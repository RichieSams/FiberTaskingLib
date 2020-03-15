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

#pragma once

#include "ftl/fiber.h"
#include "ftl/task.h"
#include "ftl/thread_abstraction.h"
#include "ftl/wait_free_queue.h"

#include <atomic>
#include <climits>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace ftl {

class AtomicCounter;

enum class EmptyQueueBehavior {
	// Fixing this will break api.
	// ReSharper disable CppInconsistentNaming

	// Spin in a loop, actively searching for tasks
	Spin,
	// Same as spin, except yields to the OS after each round of searching
	Yield,
	// Puts the thread to sleep. Will be woken when more tasks are added to the remaining awake threads.
	Sleep
	// ReSharper restore CppInconsistentNaming
};

struct TaskSchedulerInitOptions {
	/* The size of the fiber pool.The fiber pool is used to run new tasks when the current task is waiting on a counter */
	unsigned FiberPoolSize = 400;
	/* The size of the thread pool to run. 0 corresponds to NumHardwareThreads() */
	unsigned ThreadPoolSize = 0;
	/* The behavior of the threads after they have no work to do */
	EmptyQueueBehavior Behavior = EmptyQueueBehavior::Spin;
};

/**
 * A class that enables task-based multithreading.
 *
 * Underneath the covers, it uses fibers to allow cores to work on other tasks
 * when the current task is waiting on a synchronization atomic
 */
class TaskScheduler {
public:
	TaskScheduler();

	TaskScheduler(TaskScheduler const &) = delete;
	TaskScheduler(TaskScheduler &&) noexcept = delete;
	TaskScheduler &operator=(TaskScheduler const &) = delete;
	TaskScheduler &operator=(TaskScheduler &&) noexcept = delete;
	~TaskScheduler();

private:
	constexpr static size_t kFTLInvalidIndex = std::numeric_limits<size_t>::max();

	size_t m_numThreads{0};
	ThreadType *m_threads{nullptr};

	size_t m_fiberPoolSize{0};
	/* The backing storage for the fiber pool */
	Fiber *m_fibers{nullptr};
	/**
	 * An array of atomics, which signify if a fiber is available to be used. The indices of m_waitingFibers
	 * correspond 1 to 1 with m_fibers. So, if m_freeFibers[i] == true, then m_fibers[i] can be used.
	 * Each atomic acts as a lock to ensure that threads do not try to use the same fiber at the same time
	 */
	std::atomic<bool> *m_freeFibers{nullptr};

	Fiber *m_quitFibers{nullptr};

	std::atomic<bool> m_initialized{false};
	std::atomic<bool> m_quit{false};
	std::atomic<size_t> m_quitCount{0};

	std::atomic<EmptyQueueBehavior> m_emptyQueueBehavior{EmptyQueueBehavior::Spin};

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
		AtomicCounter *Counter;
	};

	struct PinnedWaitingFiberBundle {
		PinnedWaitingFiberBundle(size_t const fiberIndex, AtomicCounter *const counter, unsigned const targetValue)
		        : FiberIndex(fiberIndex), Counter(counter), TargetValue(targetValue) {
		}

		size_t FiberIndex;
		AtomicCounter *Counter;
		unsigned TargetValue;
	};

	struct alignas(kCacheLineSize) ThreadLocalStorage {
		ThreadLocalStorage()
		        : CurrentFiberIndex(kFTLInvalidIndex), OldFiberIndex(kFTLInvalidIndex) {
		}

	public:
		/**
		 * The current fiber implementation requires that fibers created from threads finish on the same thread where
		 * they started
		 *
		 * To accommodate this, we have save the initial fibers created in each thread, and immediately switch
		 * out of them into the general fiber pool. Once the 'mainTask' has finished, we signal all the threads to
		 * start quitting. When they receive the signal, they switch back to the ThreadFiber, allowing it to
		 * safely clean up.
		 */
		Fiber ThreadFiber;
		/* The index of the current fiber in m_fibers */
		size_t CurrentFiberIndex;
		/* The index of the previously executed fiber in m_fibers */
		size_t OldFiberIndex;
		/* Where OldFiber should be stored when we call CleanUpPoolAndWaiting() */
		FiberDestination OldFiberDestination{FiberDestination::None};
		/* The queue of waiting tasks */
		WaitFreeQueue<TaskBundle> TaskQueue;
		/* The last queue that we successfully stole from. This is an offset index from the current thread index */
		size_t LastSuccessfulSteal{1};
		std::atomic<bool> *OldFiberStoredFlag{nullptr};
		std::vector<std::pair<size_t, std::atomic<bool> *>> ReadyFibers;
		std::mutex ReadyFibersLock;
		unsigned FailedQueuePopAttempts{0};
		/**
		 * This lock is used with the CV below to put threads to sleep when there
		 * is no work to do. It also protects accesses to FailedQueuePopAttempts.
		 *
		 * We *could* use an atomic for FailedQueuePopAttempts, however, we still need
		 * to lock when changing the value, because spurious wakes of the CV could
		 * cause a thread to fail to wake up. See https://stackoverflow.com/a/36130475
		 * So, if we need to lock, there is no reason to have the overhead of an atomic as well.
		 */
		std::mutex FailedQueuePopLock;
		std::condition_variable FailedQueuePopCV;
	};
	/**
	 * c++ Thread Local Storage is, by definition, static/global. This poses some problems, such as multiple
	 * TaskScheduler instances. In addition, with the current fiber implementation, we have no way of telling the
	 * compiler to disable TLS optimizations, so we have to fake TLS anyhow.
	 *
	 * During initialization of the TaskScheduler, we create one ThreadLocalStorage instance per thread. Threads index
	 * into their storage using m_tls[GetCurrentThreadIndex()]
	 */
	ThreadLocalStorage *m_tls{nullptr};

	/**
	 * We friend AtomicCounter so we can keep AddReadyFiber() private
	 * This makes the public API cleaner
	 */
	friend class AtomicCounter;

public:
	/**
	 * Initializes the TaskScheduler and binds the current thread as the "main" thread
	 *
	 * TaskScheduler functions can *only* be called from this thread or inside tasks on the worker threads
	 *
	 * @param options    The configuration options for the TaskScheduler. See the struct defintion for more details
	 * @return           0 on sucess. One of the following error code on failure:
	 *                       -30  Init was called more than once
	 *                       -60  Failed to create worker threads
	 */
	int Init(TaskSchedulerInitOptions options = TaskSchedulerInitOptions());

	/**
	 * Adds a task to the internal queue.
	 *
	 * NOTE: This can *only* be called from the main thread or inside tasks on the worker threads
	 *
	 * @param task       The task to queue
	 * @param counter    An atomic counter corresponding to this task. Initially it will be incremented by 1. When the task
	 *                   completes, it will be decremented.
	 */
	void AddTask(Task task, AtomicCounter *counter = nullptr);
	/**
	 * Adds a group of tasks to the internal queue
	 *
	 * NOTE: This can *only* be called from the main thread or inside tasks on the worker threads
	 *
	 * @param numTasks    The number of tasks
	 * @param tasks       The tasks to queue
	 * @param counter     An atomic counter corresponding to the task group as a whole. Initially it will be incremented by
	 *                    numTasks. When each task completes, it will be decremented.
	 */
	void AddTasks(unsigned numTasks, Task const *tasks, AtomicCounter *counter = nullptr);

	/**
	 * Yields execution to another task until counter == value
	 *
	 * NOTE: This can *only* be called from the main thread or inside tasks on the worker threads
	 *
	 * @param counter             The counter to check
	 * @param value               The value to wait for
	 * @param pinToCurrentThread  If true, the task invoking this call will not resume on a different thread
	 */
	void WaitForCounter(AtomicCounter *counter, unsigned value, bool pinToCurrentThread = false);

	/**
	 * Gets the 0-based index of the current thread
	 * This is useful for m_tls[GetCurrentThreadIndex()]
	 *
	 * NOTE: This can *only* be called from the main thread or inside tasks on the worker threads
	 *
	 * We force no-inline because inlining seems to cause some tls-type caching on max optimization levels
	 * Discovered by @cwfitzgerald. Documented in issue #57
	 *
	 * @return    The index of the current thread
	 */
	FTL_NOINLINE size_t GetCurrentThreadIndex();

	/**
	 * Gets the amount of backing threads.
	 *
	 * @return    Backing thread count
	 */
	size_t GetThreadCount() const noexcept {
		return m_numThreads;
	}

	/**
	 * Gets the amount of fibers in the fiber pool.
	 *
	 * @return    Fiber pool size
	 */
	size_t GetFiberCount() const noexcept {
		return m_fiberPoolSize;
	}

	/**
	 * Set the behavior for how worker threads handle an empty queue
	 *
	 * @param behavior
	 * @return
	 */
	void SetEmptyQueueBehavior(EmptyQueueBehavior const behavior) {
		m_emptyQueueBehavior.store(behavior, std::memory_order_relaxed);
	}

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
	 * Gets the index of the next available fiber in the pool
	 *
	 * @return    The index of the next available fiber in the pool
	 */
	size_t GetNextFreeFiberIndex() const;
	/**
	 * If necessary, moves the old fiber to the fiber pool or the waiting list
	 * The old fiber is the last fiber to run on the thread before the current fiber
	 */
	void CleanUpOldFiber();

	/**
	 * Add a fiber to the "ready list". Fibers in the ready list will be resumed the next time a fiber goes searching
	 * for a new task
	 *
	 * @param pinnedThreadIndex    The index of the thread this fiber is pinned to. If not pinned, this will equal
	 * std::numeric_limits<size_t>::max()
	 * @param fiberIndex           The index of the fiber to add
	 * @param fiberStoredFlag      A flag used to signal if the fiber has been successfully switched out of and "cleaned
	 * up"
	 */
	void AddReadyFiber(size_t pinnedThreadIndex, size_t fiberIndex, std::atomic<bool> *fiberStoredFlag);

	/**
	 * The threadProc function for all worker threads
	 *
	 * @param arg    An instance of ThreadStartArgs
	 * @return       The return status of the thread
	 */
	static FTL_THREAD_FUNC_DECL ThreadStartFunc(void *arg);
	/**
	 * The fiberProc function for all fibers in the fiber pool
	 *
	 * @param arg    An instance of TaskScheduler
	 */
	static void FiberStartFunc(void *arg);
	/**
	 * The fiberProc function that fibers will jump to when Term() is called
	 * This allows us to jump back to the worker thread original stacks and clean up
	 * In addition, this allows the "main thread" to jump back to the "main thread" stack
	 *
	 * @param arg    An instance of ThreadTermArgs struct
	 */
	static void ThreadEndFunc(void *arg);
};

} // End of namespace ftl
