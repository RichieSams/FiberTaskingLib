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

constexpr static size_t kFailedPopAttemptsHeuristic = 5;
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

	// Get a free fiber to switch to
	size_t const freeFiberIndex = taskScheduler->GetNextFreeFiberIndex();

	// Initialize tls
	taskScheduler->m_tls[index].CurrentFiberIndex = freeFiberIndex;
	// Switch
	taskScheduler->m_tls[index].ThreadFiber.SwitchToFiber(&taskScheduler->m_fibers[freeFiberIndex]);

	// And we've returned

	// Cleanup and shutdown
	EndCurrentThread();
	FTL_THREAD_FUNC_END;
}

void TaskScheduler::FiberStartFunc(void *const arg) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	// If we just started from the pool, we may need to clean up from another fiber
	taskScheduler->CleanUpOldFiber();

	// Process tasks infinitely, until quit
	while (!taskScheduler->m_quit.load(std::memory_order_acquire)) {
		// Check if there are any ready fibers
		size_t waitingFiberIndex = kFTLInvalidIndex;
		ThreadLocalStorage &tls = taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];

		{
			std::lock_guard<std::mutex> guard(tls.ReadyFibersLock);

			for (auto iter = tls.ReadyFibers.begin(); iter != tls.ReadyFibers.end(); ++iter) {
				if (!iter->second->load(std::memory_order_relaxed)) {
					continue;
				}

				waitingFiberIndex = iter->first;
				delete iter->second;
				tls.ReadyFibers.erase(iter);
				break;
			}
		}

		if (waitingFiberIndex != kFTLInvalidIndex) {
			// Found a waiting task that is ready to continue

			tls.OldFiberIndex = tls.CurrentFiberIndex;
			tls.CurrentFiberIndex = waitingFiberIndex;
			tls.OldFiberDestination = FiberDestination::ToPool;

			// Switch
			taskScheduler->m_fibers[tls.OldFiberIndex].SwitchToFiber(&taskScheduler->m_fibers[tls.CurrentFiberIndex]);

			// And we're back
			taskScheduler->CleanUpOldFiber();

			if (taskScheduler->m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed) == EmptyQueueBehavior::Sleep) {
				std::unique_lock<std::mutex> lock(tls.FailedQueuePopLock);
				tls.FailedQueuePopAttempts = 0;
			}
		} else {
			// Get a new task from the queue, and execute it
			TaskBundle nextTask{};
			bool const success = taskScheduler->GetNextTask(&nextTask);
			EmptyQueueBehavior const behavior = taskScheduler->m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed);

			if (success) {
				if (behavior == EmptyQueueBehavior::Sleep) {
					std::unique_lock<std::mutex> lock(tls.FailedQueuePopLock);
					tls.FailedQueuePopAttempts = 0;
				}

				nextTask.TaskToExecute.Function(taskScheduler, nextTask.TaskToExecute.ArgData);
				if (nextTask.Counter != nullptr) {
					nextTask.Counter->FetchSub(1);
				}
			} else {
				// We failed to find a Task from any of the queues
				// What we do now depends on m_emptyQueueBehavior, which we loaded above
				switch (behavior) {
				case EmptyQueueBehavior::Yield:
					YieldThread();
					break;

				case EmptyQueueBehavior::Sleep: {
					std::unique_lock<std::mutex> lock(tls.FailedQueuePopLock);

					// Check if we have a ready fiber
					{
						std::lock_guard<std::mutex> guard(tls.ReadyFibersLock);

						// Prevent sleepy-time if we have ready fibers
						if (tls.ReadyFibers.empty()) {
							++tls.FailedQueuePopAttempts;
						}
					}
					// Go to sleep if we've failed to find a task kFailedPopAttemptsHeuristic times
					while (tls.FailedQueuePopAttempts >= kFailedPopAttemptsHeuristic) {
						tls.FailedQueuePopCV.wait(lock);
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
	size_t index = taskScheduler->GetCurrentThreadIndex();
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
	size_t threadIndex = taskScheduler->GetCurrentThreadIndex();

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
	FTL_VALGRIND_HG_DISABLE_CHECKING(&m_mainThreadIsBound, sizeof(m_mainThreadIsBound));
}

int TaskScheduler::Init(TaskSchedulerInitOptions options) {
	// Sanity check to make sure the user doesn't double init
	if (m_initialized.load()) {
		return kInitErrorDoubleCall;
	}

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
	FTL_VALGRIND_HG_DISABLE_CHECKING(m_freeFibers, sizeof(std::atomic<bool>) * fiberPoolSize);

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
	for (size_t i = 1; i < m_numThreads; ++i) {
		auto *const threadArgs = new ThreadStartArgs();
		threadArgs->Scheduler = this;
		threadArgs->ThreadIndex = static_cast<unsigned>(i);

		char threadName[256];
		snprintf(threadName, sizeof(threadName), "FTL Worker Thread %zu", i);

		if (!CreateThread(524288, ThreadStartFunc, threadArgs, threadName, &m_threads[i])) {
			return kInitErrorFailedToCreateWorkerThread;
		}
	}

	// Signal the worker threads that we're fully initialized
	m_initialized.store(true, std::memory_order_release);

	return 0;
}

TaskScheduler::~TaskScheduler() {
	// Create the quit fibers
	m_quitFibers = new Fiber[m_numThreads];
	for (size_t i = 0; i < m_numThreads; ++i) {
		m_quitFibers[i] = Fiber(524288, ThreadEndFunc, this);
	}

	// Request that all the threads quit
	m_quit.store(true, std::memory_order_release);

	// Signal any waiting threads so they can finish
	if (m_emptyQueueBehavior.load(std::memory_order_relaxed) == EmptyQueueBehavior::Sleep) {
		for (size_t i = 0; i < m_numThreads; ++i) {
			{
				std::unique_lock<std::mutex> lock(m_tls[i].FailedQueuePopLock);
				m_tls[i].FailedQueuePopAttempts = 0;
			}
			m_tls[i].FailedQueuePopCV.notify_all();
		}
	}

	// Jump to the quit fiber
	// Create a scope so index isn't used after we come back from the switch. It will be wrong if we started on a non-main thread
	{
		size_t index = GetCurrentThreadIndex();
		m_fibers[m_tls[index].CurrentFiberIndex].SwitchToFiber(&m_quitFibers[index]);
	}

	// We're back. We should be on the main thread now

	// Wait for the worker threads to finish
	for (size_t i = 1; i < m_numThreads; ++i) {
		JoinThread(m_threads[i]);
	}

	// Cleanup
	delete[] m_fibers;
	delete[] m_freeFibers;
	delete[] m_tls;
	delete[] m_threads;

	delete[] m_quitFibers;
}

void TaskScheduler::AddTask(Task const task, AtomicCounter *const counter) {
	if (counter != nullptr) {
		counter->FetchAdd(1);
	}

	const TaskBundle bundle = {task, counter};
	m_tls[GetCurrentThreadIndex()].TaskQueue.Push(bundle);

	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Find a thread that is sleeping and wake it
		for (size_t i = 0; i < m_numThreads; ++i) {
			std::unique_lock<std::mutex> lock(m_tls[i].FailedQueuePopLock);
			if (m_tls[i].FailedQueuePopAttempts >= kFailedPopAttemptsHeuristic) {
				m_tls[i].FailedQueuePopAttempts = 0;
				m_tls[i].FailedQueuePopCV.notify_one();

				break;
			}
		}
	}
}

void TaskScheduler::AddTasks(unsigned const numTasks, Task const *const tasks, AtomicCounter *const counter) {
	if (counter != nullptr) {
		counter->FetchAdd(numTasks);
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	for (unsigned i = 0; i < numTasks; ++i) {
		const TaskBundle bundle = {tasks[i], counter};
		tls.TaskQueue.Push(bundle);
	}

	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Wake all the threads
		for (size_t i = 0; i < m_numThreads; ++i) {
			{
				std::unique_lock<std::mutex> lock(m_tls[i].FailedQueuePopLock);
				m_tls[i].FailedQueuePopAttempts = 0;
			}
			m_tls[i].FailedQueuePopCV.notify_all();
		}
	}
}

#if defined(FTL_WIN32_THREADS)

FTL_NOINLINE size_t TaskScheduler::GetCurrentThreadIndex() {
	DWORD const threadId = ::GetCurrentThreadId();
	for (size_t i = 0; i < m_numThreads; ++i) {
		if (m_threads[i].Id == threadId) {
			return i;
		}
	}

	return kFTLInvalidIndex;
}

#elif defined(FTL_POSIX_THREADS)

FTL_NOINLINE size_t TaskScheduler::GetCurrentThreadIndex() {
	pthread_t const currentThread = pthread_self();
	for (size_t i = 0; i < m_numThreads; ++i) {
		if (pthread_equal(currentThread, m_threads[i]) != 0) {
			return i;
		}
	}

	return kFTLInvalidIndex;
}

#endif

bool TaskScheduler::GetNextTask(TaskBundle *const nextTask) {
	size_t const currentThreadIndex = GetCurrentThreadIndex();
	ThreadLocalStorage &tls = m_tls[currentThreadIndex];

	// Try to pop from our own queue
	if (tls.TaskQueue.Pop(nextTask)) {
		return true;
	}

	// Ours is empty, try to steal from the others'
	const size_t threadIndex = tls.LastSuccessfulSteal;
	for (size_t i = 0; i < m_numThreads; ++i) {
		const size_t threadIndexToStealFrom = (threadIndex + i) % m_numThreads;
		if (threadIndexToStealFrom == currentThreadIndex) {
			continue;
		}
		ThreadLocalStorage &otherTLS = m_tls[threadIndexToStealFrom];
		if (otherTLS.TaskQueue.Steal(nextTask)) {
			tls.LastSuccessfulSteal = threadIndexToStealFrom;
			return true;
		}
	}

	return false;
}

size_t TaskScheduler::GetNextFreeFiberIndex() const {
	for (unsigned j = 0;; ++j) {
		for (size_t i = 0; i < m_fiberPoolSize; ++i) {
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
		tls.OldFiberIndex = kFTLInvalidIndex;
		break;
	case FiberDestination::ToWaiting:
		// The waiting fibers are stored directly in their counters
		// They have an atomic<bool> that signals whether the waiting fiber can be consumed if it's ready
		// We just have to set it to true
		tls.OldFiberStoredFlag->store(true, std::memory_order_relaxed);
		tls.OldFiberDestination = FiberDestination::None;
		tls.OldFiberIndex = kFTLInvalidIndex;
		break;
	case FiberDestination::None:
	default:
		break;
	}
}

void TaskScheduler::AddReadyFiber(size_t const pinnedThreadIndex, size_t fiberIndex, std::atomic<bool> *const fiberStoredFlag) {
	ThreadLocalStorage *tls;
	if (pinnedThreadIndex == std::numeric_limits<size_t>::max()) {
		tls = &m_tls[GetCurrentThreadIndex()];
	} else {
		tls = &m_tls[pinnedThreadIndex];
	}

	{
		std::lock_guard<std::mutex> guard(tls->ReadyFibersLock);
		tls->ReadyFibers.emplace_back(fiberIndex, fiberStoredFlag);
	}

	// If the Task is pinned, we add the Task to the pinned thread's ReadyFibers queue, instead
	// of our own. Normally, this works fine; the other thread will pick it up next time it
	// searches for a Task to run.
	//
	// However, if we're using EmptyQueueBehavior::Sleep, the other thread could be sleeping
	// Therefore, we need to kick the thread to make sure that it's awake
	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Kick the thread
		{
			std::unique_lock<std::mutex> lock(tls->FailedQueuePopLock);
			tls->FailedQueuePopAttempts = 0;
		}
		tls->FailedQueuePopCV.notify_all();
	}
}

void TaskScheduler::WaitForCounter(AtomicCounter *const counter, unsigned const value, bool const pinToCurrentThread) {
	// Fast out
	if (counter->Load(std::memory_order_relaxed) == value) {
		// wait for threads to drain from counter logic, otherwise we might continue too early
		while (counter->m_lock.load() > 0) {
		}
		return;
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	size_t const currentFiberIndex = tls.CurrentFiberIndex;

	size_t pinnedThreadIndex;
	if (pinToCurrentThread) {
		pinnedThreadIndex = GetCurrentThreadIndex();
	} else {
		pinnedThreadIndex = std::numeric_limits<size_t>::max();
	}
	auto *const fiberStoredFlag = new std::atomic<bool>(false);
	bool const alreadyDone = counter->AddFiberToWaitingList(tls.CurrentFiberIndex, value, fiberStoredFlag, pinnedThreadIndex);

	// The counter finished while we were trying to put it in the waiting list
	// Just clean up and trivially return
	if (alreadyDone) {
		delete fiberStoredFlag;
		return;
	}

	// Get a free fiber
	size_t const freeFiberIndex = GetNextFreeFiberIndex();

	// Fill in tls
	tls.OldFiberIndex = currentFiberIndex;
	tls.CurrentFiberIndex = freeFiberIndex;
	tls.OldFiberDestination = FiberDestination::ToWaiting;
	tls.OldFiberStoredFlag = fiberStoredFlag;

	// Switch
	m_fibers[currentFiberIndex].SwitchToFiber(&m_fibers[freeFiberIndex]);

	// And we're back
	CleanUpOldFiber();
}

} // End of namespace ftl
