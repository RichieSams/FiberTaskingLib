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

#include "ftl/task_scheduler.h"

#include "ftl/atomic_counter.h"
#include "ftl/callbacks.h"
#include "ftl/task_counter.h"
#include "ftl/thread_abstraction.h"

#if defined(FTL_WIN32_THREADS)
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>
#elif defined(FTL_POSIX_THREADS)
#	include <pthread.h>
#endif

namespace ftl {

constexpr static unsigned kFailedPopAttemptsHeuristic = 5;
constexpr static int kInitErrorDoubleCall = -30;
constexpr static int kInitErrorFailedToCreateWorkerThread = -60;

struct ThreadStartArgs {
	TaskScheduler *Scheduler;
	unsigned ThreadIndex;
};

FTL_THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStartFunc(void *const arg) {
	auto *const threadArgs = reinterpret_cast<ThreadStartArgs *>(arg);
	TaskScheduler *taskScheduler = threadArgs->Scheduler;
	unsigned const index = threadArgs->ThreadIndex;

	// Clean up
	delete threadArgs;

	// Spin wait until everything is initialized
	while (!taskScheduler->m_initialized.load(std::memory_order_acquire)) {
		// Spin
		FTL_PAUSE();
	}

	// Execute user thread start callback, if set
	const EventCallbacks &callbacks = taskScheduler->m_callbacks;
	if (callbacks.OnWorkerThreadStarted != nullptr) {
		callbacks.OnWorkerThreadStarted(callbacks.Context, index);
	}

	// Get a free fiber to switch to
	unsigned const freeFiberIndex = taskScheduler->GetNextFreeFiberIndex();

	// Initialize tls
	taskScheduler->m_tls[index].CurrentFiberIndex = freeFiberIndex;
	// Switch
	taskScheduler->m_tls[index].ThreadFiber.SwitchToFiber(&taskScheduler->m_fibers[freeFiberIndex]);

	// And we've returned

	// Execute user thread end callback, if set
	if (callbacks.OnWorkerThreadEnded != nullptr) {
		callbacks.OnWorkerThreadEnded(callbacks.Context, index);
	}

	// Cleanup and shutdown
	EndCurrentThread();
	FTL_THREAD_FUNC_END;
}

// This Task is never used directly
// However, a function pointer to it is the signal that the task is a Ready fiber, not a "real" task
// See @FiberStartFunc() for more details
static void ReadyFiberDummyTask(TaskScheduler *taskScheduler, void *arg) {
	(void)taskScheduler;
	(void)arg;
}

void TaskScheduler::FiberStartFunc(void *const arg) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	if (taskScheduler->m_callbacks.OnFiberAttached != nullptr) {
		taskScheduler->m_callbacks.OnFiberAttached(taskScheduler->m_callbacks.Context, taskScheduler->GetCurrentFiberIndex());
	}

	// If we just started from the pool, we may need to clean up from another fiber
	taskScheduler->CleanUpOldFiber();

	std::vector<TaskBundle> taskBuffer;

	// Process tasks infinitely, until quit
	while (!taskScheduler->m_quit.load(std::memory_order_acquire)) {
		unsigned waitingFiberIndex = kInvalidIndex;
		ThreadLocalStorage *tls = &taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];

		bool readyWaitingFibers = false;

		// Check if there is a ready pinned waiting fiber
		{
			std::lock_guard<std::mutex> guard(tls->PinnedReadyFibersLock);

			for (auto bundle = tls->PinnedReadyFibers.begin(); bundle != tls->PinnedReadyFibers.end(); ++bundle) {
				readyWaitingFibers = true;

				if (!(*bundle)->FiberIsSwitched.load(std::memory_order_acquire)) {
					// The wait condition is ready, but the "source" thread hasn't switched away from the fiber yet
					// Skip this fiber until the next round
					continue;
				}

				waitingFiberIndex = (*bundle)->FiberIndex;
				tls->PinnedReadyFibers.erase(bundle);
				break;
			}
		}

		TaskBundle nextTask{};
		bool foundTask = false;

		// If nothing was found, check if there is a high priority task to run
		if (waitingFiberIndex == kInvalidIndex) {
			foundTask = taskScheduler->GetNextHiPriTask(&nextTask, &taskBuffer);

			// Check if the found task is a ReadyFiber dummy task
			if (foundTask && nextTask.TaskToExecute.Function == ReadyFiberDummyTask) {
				// Get the waiting fiber index
				ReadyFiberBundle *readyFiberBundle = reinterpret_cast<ReadyFiberBundle *>(nextTask.TaskToExecute.ArgData);
				waitingFiberIndex = readyFiberBundle->FiberIndex;
			}
		}

		if (waitingFiberIndex != kInvalidIndex) {
			// Found a waiting task that is ready to continue

			tls->OldFiberIndex = tls->CurrentFiberIndex;
			tls->CurrentFiberIndex = waitingFiberIndex;
			tls->OldFiberDestination = FiberDestination::ToPool;

			const EventCallbacks &callbacks = taskScheduler->m_callbacks;
			if (callbacks.OnFiberDetached != nullptr) {
				callbacks.OnFiberDetached(callbacks.Context, tls->OldFiberIndex, false);
			}

			// Switch
			taskScheduler->m_fibers[tls->OldFiberIndex].SwitchToFiber(&taskScheduler->m_fibers[tls->CurrentFiberIndex]);

			if (callbacks.OnFiberAttached != nullptr) {
				callbacks.OnFiberAttached(callbacks.Context, taskScheduler->GetCurrentFiberIndex());
			}

			// And we're back
			taskScheduler->CleanUpOldFiber();

			// Get a fresh instance of TLS, since we could be on a new thread now
			tls = &taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];

			if (taskScheduler->m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed) == EmptyQueueBehavior::Sleep) {
				tls->FailedQueuePopAttempts = 0;
			}
		} else {
			// If we didn't find a high priority task, look for a low priority task
			if (!foundTask) {
				foundTask = taskScheduler->GetNextLoPriTask(&nextTask);
			}

			EmptyQueueBehavior const behavior = taskScheduler->m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed);

			if (foundTask) {
				if (behavior == EmptyQueueBehavior::Sleep) {
					tls->FailedQueuePopAttempts = 0;
				}

				nextTask.TaskToExecute.Function(taskScheduler, nextTask.TaskToExecute.ArgData);
				if (nextTask.Counter != nullptr) {
					nextTask.Counter->Decrement();
				}
			} else {
				// We failed to find a Task from any of the queues
				// What we do now depends on m_emptyQueueBehavior, which we loaded above
				switch (behavior) {
				case EmptyQueueBehavior::Yield:
					YieldThread();
					break;

				case EmptyQueueBehavior::Sleep: {
					// If we have a ready waiting fiber, prevent sleep
					if (!readyWaitingFibers) {
						++tls->FailedQueuePopAttempts;
						// Go to sleep if we've failed to find a task kFailedPopAttemptsHeuristic times
						if (tls->FailedQueuePopAttempts >= kFailedPopAttemptsHeuristic) {
							std::unique_lock<std::mutex> lock(taskScheduler->ThreadSleepLock);
							// Acquire the pinned ready fibers lock here and check if there are any pinned fibers ready
							// Acquiring the lock here prevents a race between readying a pinned fiber (on another thread) and going to sleep
							// Either this thread wins, then notify_*() will wake it
							// Or the other thread wins, then this thread will observe the pinned fiber, and will not go to sleep
							std::unique_lock<std::mutex> readyfiberslock(tls->PinnedReadyFibersLock);
							if (tls->PinnedReadyFibers.empty()) {
								// Unlock before going to sleep (the other lock is released by the CV wait)
								readyfiberslock.unlock();
								taskScheduler->ThreadSleepCV.wait(lock);
							}
							tls->FailedQueuePopAttempts = 0;
						}
					}

					break;
				}
				case EmptyQueueBehavior::Spin:
				default:
					// Just fall through and continue the next loop
					break;
				}
			}
		}
	}

	// Switch to the quit fibers

	if (taskScheduler->m_callbacks.OnFiberDetached != nullptr) {
		taskScheduler->m_callbacks.OnFiberDetached(taskScheduler->m_callbacks.Context, taskScheduler->GetCurrentFiberIndex(), false);
	}

	unsigned index = taskScheduler->GetCurrentThreadIndex();
	taskScheduler->m_fibers[taskScheduler->m_tls[index].CurrentFiberIndex].SwitchToFiber(&taskScheduler->m_quitFibers[index]);

	// We should never get here
	printf("Error: FiberStart should never return");
}

void TaskScheduler::ThreadEndFunc(void *arg) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	// Wait for all other threads to quit
	taskScheduler->m_quitCount.fetch_add(1, std::memory_order_seq_cst);
	while (taskScheduler->m_quitCount.load(std::memory_order_seq_cst) != taskScheduler->m_numThreads) {
		SleepThread(50);
	}

	// Jump to the thread fibers
	unsigned threadIndex = taskScheduler->GetCurrentThreadIndex();

	if (threadIndex == 0) {
		// Special case for the main thread fiber
		taskScheduler->m_quitFibers[threadIndex].SwitchToFiber(&taskScheduler->m_fibers[0]);
	} else {
		taskScheduler->m_quitFibers[threadIndex].SwitchToFiber(&taskScheduler->m_tls[threadIndex].ThreadFiber);
	}

	// We should never get here
	printf("Error: ThreadEndFunc should never return");
}

TaskScheduler::TaskScheduler() {
	FTL_VALGRIND_HG_DISABLE_CHECKING(&m_initialized, sizeof(m_initialized));
	FTL_VALGRIND_HG_DISABLE_CHECKING(&m_quit, sizeof(m_quit));
	FTL_VALGRIND_HG_DISABLE_CHECKING(&m_quitCount, sizeof(m_quitCount));
}

int TaskScheduler::Init(TaskSchedulerInitOptions options) {
	// Sanity check to make sure the user doesn't double init
	if (m_initialized.load()) {
		return kInitErrorDoubleCall;
	}

	m_callbacks = options.Callbacks;

	// Initialize the flags
	m_emptyQueueBehavior.store(options.Behavior);

	if (options.ThreadPoolSize == 0) {
		// 1 thread for each logical processor
		m_numThreads = GetNumHardwareThreads();
	} else {
		m_numThreads = options.ThreadPoolSize;
	}

	// Create and populate the fiber pool
	m_fiberPoolSize = options.FiberPoolSize;
	m_fibers = new Fiber[options.FiberPoolSize];
	m_freeFibers = new std::atomic<bool>[options.FiberPoolSize];
	FTL_VALGRIND_HG_DISABLE_CHECKING(m_freeFibers, sizeof(std::atomic<bool>) * m_fiberPoolSize);
	m_readyFiberBundles = new ReadyFiberBundle[options.FiberPoolSize];

	// Leave the first slot for the bound main thread
	for (unsigned i = 1; i < options.FiberPoolSize; ++i) {
		m_fibers[i] = Fiber(524288, FiberStartFunc, this);
		m_freeFibers[i].store(true, std::memory_order_release);
	}
	m_freeFibers[0].store(false, std::memory_order_release);

	// Initialize threads and TLS
	m_threads = new ThreadType[m_numThreads];
#ifdef _MSC_VER
#	pragma warning(push)
#	pragma warning(disable : 4316) // I know this won't be allocated to the right alignment, this is okay as we're using alignment for padding.
#endif                              // _MSC_VER
	m_tls = new ThreadLocalStorage[m_numThreads];
#ifdef _MSC_VER
#	pragma warning(pop)
#endif // _MSC_VER

#if defined(FTL_WIN32_THREADS)
	// Temporarily set the main thread ID to -1, so when the worker threads start up, they don't accidentally use it
	// I don't know if Windows thread id's can ever be 0, but just in case.
	m_threads[0].Id = static_cast<DWORD>(-1);
#endif

	if (m_callbacks.OnThreadsCreated != nullptr) {
		m_callbacks.OnThreadsCreated(m_callbacks.Context, m_numThreads);
	}
	if (m_callbacks.OnFibersCreated != nullptr) {
		m_callbacks.OnFibersCreated(m_callbacks.Context, options.FiberPoolSize);
	}

	// Set the properties for the current thread
	SetCurrentThreadAffinity(0);
	m_threads[0] = GetCurrentThread();
#if defined(FTL_WIN32_THREADS)
	// Set the thread handle to INVALID_HANDLE_VALUE
	// ::GetCurrentThread is a pseudo handle, that always references the current thread.
	// Aka, if we tried to use this handle from another thread to reference the main thread,
	// it would instead reference the other thread. We don't currently use the handle anywhere.
	// Therefore, we set this to INVALID_HANDLE_VALUE, so any future usages can take this into account
	// Reported by @rtj
	m_threads[0].Handle = INVALID_HANDLE_VALUE;
#endif

	// Set the fiber index
	m_tls[0].CurrentFiberIndex = 0;

	// Create the worker threads
	for (unsigned i = 1; i < m_numThreads; ++i) {
		auto *const threadArgs = new ThreadStartArgs();
		threadArgs->Scheduler = this;
		threadArgs->ThreadIndex = i;

		char threadName[256];
		snprintf(threadName, sizeof(threadName), "FTL Worker Thread %u", i);

		if (!CreateThread(524288, ThreadStartFunc, threadArgs, threadName, &m_threads[i])) {
			return kInitErrorFailedToCreateWorkerThread;
		}
	}

	// Manually invoke callback for 'main' fiber
	if (m_callbacks.OnFiberAttached != nullptr) {
		m_callbacks.OnFiberAttached(m_callbacks.Context, 0);
	}

	// Signal the worker threads that we're fully initialized
	m_initialized.store(true, std::memory_order_release);

	return 0;
}

TaskScheduler::~TaskScheduler() {
	// Create the quit fibers
	m_quitFibers = new Fiber[m_numThreads];
	for (unsigned i = 0; i < m_numThreads; ++i) {
		m_quitFibers[i] = Fiber(524288, ThreadEndFunc, this);
	}

	// Request that all the threads quit
	m_quit.store(true, std::memory_order_release);

	// Signal any waiting threads so they can finish
	if (m_emptyQueueBehavior.load(std::memory_order_relaxed) == EmptyQueueBehavior::Sleep) {
		ThreadSleepCV.notify_all();
	}

	// Jump to the quit fiber
	// Create a scope so index isn't used after we come back from the switch. It will be wrong if we started on a non-main thread
	{
		if (m_callbacks.OnFiberDetached != nullptr) {
			m_callbacks.OnFiberDetached(m_callbacks.Context, GetCurrentFiberIndex(), false);
		}

		unsigned index = GetCurrentThreadIndex();
		m_fibers[m_tls[index].CurrentFiberIndex].SwitchToFiber(&m_quitFibers[index]);
	}

	// We're back. We should be on the main thread now

	// Wait for the worker threads to finish
	for (unsigned i = 1; i < m_numThreads; ++i) {
		JoinThread(m_threads[i]);
	}

	// Cleanup
	delete[] m_tls;
	delete[] m_threads;
	delete[] m_readyFiberBundles;
	delete[] m_freeFibers;
	delete[] m_fibers;

	delete[] m_quitFibers;
}

void TaskScheduler::AddTask(Task const task, TaskPriority priority, TaskCounter *const counter) {
	FTL_ASSERT("Task given to TaskScheduler:AddTask has a nullptr Function", task.Function != nullptr);

	if (counter != nullptr) {
		counter->Add(1);
	}

	const TaskBundle bundle = {task, counter};
	if (priority == TaskPriority::High) {
		m_tls[GetCurrentThreadIndex()].HiPriTaskQueue.Push(bundle);
	} else if (priority == TaskPriority::Normal) {
		m_tls[GetCurrentThreadIndex()].LoPriTaskQueue.Push(bundle);
	}

	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Wake a sleeping thread
		ThreadSleepCV.notify_one();
	}
}

void TaskScheduler::AddTasks(unsigned const numTasks, Task const *const tasks, TaskPriority priority, TaskCounter *const counter) {
	if (counter != nullptr) {
		counter->Add(numTasks);
	}

	WaitFreeQueue<TaskBundle> *queue = nullptr;
	if (priority == TaskPriority::High) {
		queue = &m_tls[GetCurrentThreadIndex()].HiPriTaskQueue;
	} else if (priority == TaskPriority::Normal) {
		queue = &m_tls[GetCurrentThreadIndex()].LoPriTaskQueue;
	} else {
		FTL_ASSERT("Unknown task priority", false);
		return;
	}
	for (unsigned i = 0; i < numTasks; ++i) {
		FTL_ASSERT("Task given to TaskScheduler:AddTasks has a nullptr Function", tasks[i].Function != nullptr);
		const TaskBundle bundle = {tasks[i], counter};
		queue->Push(bundle);
	}

	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Wake all the threads
		ThreadSleepCV.notify_all();
	}
}

#if defined(FTL_WIN32_THREADS)

FTL_NOINLINE unsigned TaskScheduler::GetCurrentThreadIndex() const {
	DWORD const threadId = ::GetCurrentThreadId();
	for (unsigned i = 0; i < m_numThreads; ++i) {
		if (m_threads[i].Id == threadId) {
			return i;
		}
	}

	return kInvalidIndex;
}

#elif defined(FTL_POSIX_THREADS)

FTL_NOINLINE unsigned TaskScheduler::GetCurrentThreadIndex() const {
	pthread_t const currentThread = pthread_self();
	for (unsigned i = 0; i < m_numThreads; ++i) {
		if (pthread_equal(currentThread, m_threads[i]) != 0) {
			return i;
		}
	}

	return kInvalidIndex;
}

#endif

unsigned TaskScheduler::GetCurrentFiberIndex() const {
	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	return tls.CurrentFiberIndex;
}

inline bool TaskScheduler::TaskIsReadyToExecute(TaskBundle *bundle) const {
	// "Real" tasks are always ready to execute
	if (bundle->TaskToExecute.Function != ReadyFiberDummyTask) {
		return true;
	}

	// If it's a ready fiber task, the arg is a ReadyFiberBundle
	ReadyFiberBundle *readyFiberBundle = reinterpret_cast<ReadyFiberBundle *>(bundle->TaskToExecute.ArgData);
	return readyFiberBundle->FiberIsSwitched.load(std::memory_order_acquire);
}

bool TaskScheduler::GetNextHiPriTask(TaskBundle *nextTask, std::vector<TaskBundle> *taskBuffer) {
	unsigned const currentThreadIndex = GetCurrentThreadIndex();
	ThreadLocalStorage &tls = m_tls[currentThreadIndex];

	bool result = false;

	// Try to pop from our own queue
	while (tls.HiPriTaskQueue.Pop(nextTask)) {
		if (TaskIsReadyToExecute(nextTask)) {
			result = true;
			// Break to cleanup
			goto cleanup; // NOLINT(cppcoreguidelines-avoid-goto)
		}

		// It's a ReadyTask whose fiber hasn't switched away yet
		// Add it to the buffer
		taskBuffer->emplace_back(*nextTask);
	}

	// Force a scope so the `goto cleanup` above doesn't skip initialization
	{
		// Ours is empty, try to steal from the others'
		const unsigned threadIndex = tls.HiPriLastSuccessfulSteal;
		for (unsigned i = 0; i < m_numThreads; ++i) {
			const unsigned threadIndexToStealFrom = (threadIndex + i) % m_numThreads;
			if (threadIndexToStealFrom == currentThreadIndex) {
				continue;
			}
			ThreadLocalStorage &otherTLS = m_tls[threadIndexToStealFrom];

			while (otherTLS.HiPriTaskQueue.Steal(nextTask)) {
				tls.HiPriLastSuccessfulSteal = threadIndexToStealFrom;

				if (TaskIsReadyToExecute(nextTask)) {
					result = true;
					// Break to cleanup
					goto cleanup;
				}

				// It's a ReadyTask whose fiber hasn't switched away yet
				// Add it to the buffer
				taskBuffer->emplace_back(*nextTask);
			}
		}
	}

cleanup:
	if (!taskBuffer->empty()) {
		// Re-push all the tasks we found that we're ready to execute
		// We (or another thread) will get them next round
		do {
			// Push them in the opposite order we popped them, to restore the order
			tls.HiPriTaskQueue.Push(taskBuffer->back());
			taskBuffer->pop_back();
		} while (!taskBuffer->empty());

		// If we're using Sleep mode, we need to wake up the other threads
		// They may have looked for tasks while we had them all in our temp buffer and thus not
		// found anything and gone to sleep.
		EmptyQueueBehavior const behavior = m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed);
		if (behavior == EmptyQueueBehavior::Sleep) {
			// Wake all the threads
			ThreadSleepCV.notify_all();
		}
	}

	return result;
}

bool TaskScheduler::GetNextLoPriTask(TaskBundle *nextTask) {
	unsigned const currentThreadIndex = GetCurrentThreadIndex();
	ThreadLocalStorage &tls = m_tls[currentThreadIndex];

	// Try to pop from our own queue
	if (tls.LoPriTaskQueue.Pop(nextTask)) {
		return true;
	}

	// Ours is empty, try to steal from the others'
	const unsigned threadIndex = tls.LoPriLastSuccessfulSteal;
	for (unsigned i = 0; i < m_numThreads; ++i) {
		const unsigned threadIndexToStealFrom = (threadIndex + i) % m_numThreads;
		if (threadIndexToStealFrom == currentThreadIndex) {
			continue;
		}
		ThreadLocalStorage &otherTLS = m_tls[threadIndexToStealFrom];
		if (otherTLS.LoPriTaskQueue.Steal(nextTask)) {
			tls.LoPriLastSuccessfulSteal = threadIndexToStealFrom;
			return true;
		}
	}

	return false;
}

unsigned TaskScheduler::GetNextFreeFiberIndex() const {
	for (unsigned j = 0;; ++j) {
		for (unsigned i = 0; i < m_fiberPoolSize; ++i) {
			// Double lock
			if (!m_freeFibers[i].load(std::memory_order_relaxed)) {
				continue;
			}

			if (!m_freeFibers[i].load(std::memory_order_acquire)) {
				continue;
			}

			bool expected = true;
			if (std::atomic_compare_exchange_weak_explicit(&m_freeFibers[i], &expected, false, std::memory_order_release, std::memory_order_relaxed)) {
				return i;
			}
		}

		if (j > 10) {
			printf("No free fibers in the pool. Possible deadlock");
		}
	}
}

void TaskScheduler::CleanUpOldFiber() {
	// Clean up from the last Fiber to run on this thread
	//
	// Explanation:
	// When switching between fibers, there's the innate problem of tracking the fibers.
	// For example, let's say we discover a waiting fiber that's ready. We need to put the currently
	// running fiber back into the fiber pool, and then switch to the waiting fiber. However, we can't
	// just do the equivalent of:
	//     m_fibers.Push(currentFiber)
	//     currentFiber.SwitchToFiber(waitingFiber)
	// In the time between us adding the current fiber to the fiber pool and switching to the waiting fiber, another
	// thread could come along and pop the current fiber from the fiber pool and try to run it.
	// This leads to stack corruption and/or other undefined behavior.
	//
	// In the previous implementation of TaskScheduler, we used helper fibers to do this work for us.
	// AKA, we stored currentFiber and waitingFiber in TLS, and then switched to the helper fiber. The
	// helper fiber then did:
	//     m_fibers.Push(currentFiber)
	//     helperFiber.SwitchToFiber(waitingFiber)
	// If we have 1 helper fiber per thread, we can guarantee that currentFiber is free to be executed by any thread
	// once it is added back to the fiber pool
	//
	// This solution works well, however, we actually don't need the helper fibers
	// The code structure guarantees that between any two fiber switches, the code will always end up in WaitForCounter
	// or FiberStart. Therefore, instead of using a helper fiber and immediately pushing the fiber to the fiber pool or
	// waiting list, we defer the push until the next fiber gets to one of those two places
	//
	// Proof:
	// There are only two places where we switch fibers:
	// 1. When we're waiting for a counter, we pull a new fiber from the fiber pool and switch to it.
	// 2. When we found a counter that's ready, we put the current fiber back in the fiber pool, and switch to the
	// waiting fiber.
	//
	// Case 1:
	// A fiber from the pool will always either be completely new or just come back from switching to a waiting fiber
	// In both places, we call CleanUpOldFiber()
	// QED
	//
	// Case 2:
	// A waiting fiber will always resume in WaitForCounter()
	// Here, we call CleanUpOldFiber()
	// QED

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	switch (tls.OldFiberDestination) {
	case FiberDestination::ToPool:
		// In this specific implementation, the fiber pool is a flat array signaled by atomics
		// So in order to "Push" the fiber to the fiber pool, we just set its corresponding atomic to true
		m_freeFibers[tls.OldFiberIndex].store(true, std::memory_order_release);
		tls.OldFiberDestination = FiberDestination::None;
		tls.OldFiberIndex = kInvalidIndex;
		break;
	case FiberDestination::ToWaiting:
		// The waiting fibers are stored directly in their counters
		// They have an atomic<bool> that signals whether the waiting fiber can be consumed if it's ready
		// We just have to set it to true
		tls.OldFiberStoredFlag->store(true, std::memory_order_release);
		tls.OldFiberDestination = FiberDestination::None;
		tls.OldFiberIndex = kInvalidIndex;
		break;
	case FiberDestination::None:
	default:
		break;
	}
}

void TaskScheduler::AddReadyFiber(unsigned const pinnedThreadIndex, ReadyFiberBundle *bundle) {
	if (pinnedThreadIndex == kNoThreadPinning) {
		ThreadLocalStorage *tls = &m_tls[GetCurrentThreadIndex()];

		// Push a dummy task to the high priority queue
		Task task{};
		task.Function = ReadyFiberDummyTask;
		task.ArgData = bundle;
		TaskBundle taskBundle{};
		taskBundle.TaskToExecute = task;
		taskBundle.Counter = nullptr;

		tls->HiPriTaskQueue.Push(taskBundle);

		// If we're using EmptyQueueBehavior::Sleep, the other threads could be sleeping
		// Therefore, we need to kick a thread awake to ensure that the readied task is taken
		const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
		if (behavior == EmptyQueueBehavior::Sleep) {
			ThreadSleepCV.notify_one();
		}
	} else {
		ThreadLocalStorage *tls = &m_tls[pinnedThreadIndex];

		{
			std::lock_guard<std::mutex> guard(tls->PinnedReadyFibersLock);
			tls->PinnedReadyFibers.emplace_back(bundle);
		}

		// If the Task is pinned, we add the Task to the pinned thread's PinnedReadyFibers queue
		// Normally, this works fine; the other thread will pick it up next time it
		// searches for a Task to run.
		//
		// However, if we're using EmptyQueueBehavior::Sleep, the other thread could be sleeping
		// Therefore, we need to kick all the threads so that the pinned-to thread can take it
		const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed);
		if (behavior == EmptyQueueBehavior::Sleep) {
			if (GetCurrentThreadIndex() != pinnedThreadIndex) {
				std::unique_lock<std::mutex> lock(ThreadSleepLock);
				// Kick all threads
				ThreadSleepCV.notify_all();
			}
		}
	}
}

void TaskScheduler::WaitForCounter(TaskCounter *counter, bool pinToCurrentThread) {
	WaitForCounterInternal(counter, 0, pinToCurrentThread);
}

void TaskScheduler::WaitForCounter(AtomicFlag *counter, bool pinToCurrentThread) {
	WaitForCounterInternal(counter, 0, pinToCurrentThread);
}

void TaskScheduler::WaitForCounter(FullAtomicCounter *counter, unsigned value, bool pinToCurrentThread) {
	WaitForCounterInternal(counter, value, pinToCurrentThread);
}

void TaskScheduler::WaitForCounterInternal(BaseCounter *counter, unsigned value, bool pinToCurrentThread) {
	// Fast out
	if (counter->m_value.load(std::memory_order_relaxed) == value) {
		// wait for threads to drain from counter logic, otherwise we might continue too early
		while (counter->m_lock.load() > 0) {
		}
		return;
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	unsigned const currentFiberIndex = tls.CurrentFiberIndex;

	unsigned pinnedThreadIndex;
	if (pinToCurrentThread) {
		pinnedThreadIndex = GetCurrentThreadIndex();
	} else {
		pinnedThreadIndex = kNoThreadPinning;
	}

	// Create the ready fiber bundle and attempt to add it to the waiting list
	ReadyFiberBundle *readyFiberBundle = &m_readyFiberBundles[currentFiberIndex];
	readyFiberBundle->FiberIndex = currentFiberIndex;
	readyFiberBundle->FiberIsSwitched.store(false);

	bool const alreadyDone = counter->AddFiberToWaitingList(readyFiberBundle, value, pinnedThreadIndex);

	// The counter finished while we were trying to put it in the waiting list
	// Just trivially return
	if (alreadyDone) {
		return;
	}

	// Get a free fiber
	unsigned const freeFiberIndex = GetNextFreeFiberIndex();

	// Fill in tls
	tls.OldFiberIndex = currentFiberIndex;
	tls.CurrentFiberIndex = freeFiberIndex;
	tls.OldFiberDestination = FiberDestination::ToWaiting;
	tls.OldFiberStoredFlag = &readyFiberBundle->FiberIsSwitched;

	if (m_callbacks.OnFiberDetached != nullptr) {
		m_callbacks.OnFiberDetached(m_callbacks.Context, currentFiberIndex, true);
	}

	// Switch
	m_fibers[currentFiberIndex].SwitchToFiber(&m_fibers[freeFiberIndex]);

	if (m_callbacks.OnFiberAttached != nullptr) {
		m_callbacks.OnFiberAttached(m_callbacks.Context, GetCurrentFiberIndex());
	}

	// And we're back
	CleanUpOldFiber();
}

} // End of namespace ftl
