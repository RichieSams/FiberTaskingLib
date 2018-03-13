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


namespace ftl {

enum {
	kFailedPopAttemptsHeuristic = 5
};


struct ThreadStartArgs {
	TaskScheduler *taskScheduler;
	uint threadIndex;
};

FTL_THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStart(void *arg) {
	ThreadStartArgs *threadArgs = reinterpret_cast<ThreadStartArgs *>(arg);
	TaskScheduler *taskScheduler = threadArgs->taskScheduler;
	uint index = threadArgs->threadIndex;

	// Clean up
	delete threadArgs;

	// Spin wait until everything is initialized
	while (!taskScheduler->m_initialized.load(std::memory_order_acquire)) {
		// Spin
	}


	// Get a free fiber to switch to
	std::size_t freeFiberIndex = taskScheduler->GetNextFreeFiberIndex();

	// Initialize tls
	taskScheduler->m_tls[index].CurrentFiberIndex = freeFiberIndex;
	// Switch
	taskScheduler->m_tls[index].ThreadFiber.SwitchToFiber(&taskScheduler->m_fibers[freeFiberIndex]);


	// And we've returned

	// Cleanup and shutdown
	EndCurrentThread();
	FTL_THREAD_FUNC_END;
}

struct MainFiberStartArgs {
	TaskFunction MainTask;
	void *Arg;
	TaskScheduler *taskScheduler;
};

void TaskScheduler::MainFiberStart(void *arg) {
	MainFiberStartArgs *mainFiberArgs = reinterpret_cast<MainFiberStartArgs *>(arg);
	TaskScheduler *taskScheduler = mainFiberArgs->taskScheduler;

	// Call the main task procedure
	mainFiberArgs->MainTask(taskScheduler, mainFiberArgs->Arg);


	// Request that all the threads quit
	taskScheduler->m_quit.store(true, std::memory_order_release);

	// Signal any waiting threads so they can finish
	if (taskScheduler->m_emptyQueueBehavior.load(std::memory_order_relaxed) == EmptyQueueBehavior::Sleep) {
		for (uint i = 0; i < taskScheduler->m_numThreads; ++i) {
			{
				std::unique_lock<std::mutex> lock(taskScheduler->m_tls[i].FailedQueuePopLock);
				taskScheduler->m_tls[i].FailedQueuePopAttempts = 0;
			}
			taskScheduler->m_tls[i].FailedQueuePopCV.notify_all();
		}
	}

	// Switch to the thread fibers
	ThreadLocalStorage &tls = taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];
	taskScheduler->m_fibers[tls.CurrentFiberIndex].SwitchToFiber(&tls.ThreadFiber);


	// We should never get here
	printf("Error: FiberStart should never return");
}

void TaskScheduler::FiberStart(void *arg) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	// If we just started from the pool, we may need to clean up from another fiber
	taskScheduler->CleanUpOldFiber();

	while (!taskScheduler->m_quit.load(std::memory_order_acquire)) {
		// Check if there are any ready fibers
		std::size_t waitingFiberIndex = FTL_INVALID_INDEX;
		ThreadLocalStorage &tls = taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];

		// Lock
		while (tls.ReadFibersLock.test_and_set(std::memory_order_acquire)) {
			// Spin
		}

		for (auto iter = tls.ReadyFibers.begin(); iter != tls.ReadyFibers.end(); ++iter) {
			if (!iter->second->load(std::memory_order_relaxed)) {
				continue;
			}

			waitingFiberIndex = iter->first;
			delete iter->second;
			tls.ReadyFibers.erase(iter);
			break;
		}

		// Unlock
		tls.ReadFibersLock.clear(std::memory_order_release);

		if (waitingFiberIndex != FTL_INVALID_INDEX) {
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
			TaskBundle nextTask;
			bool success = taskScheduler->GetNextTask(&nextTask);
			EmptyQueueBehavior behavior = taskScheduler->m_emptyQueueBehavior.load(std::memory_order::memory_order_relaxed);

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

				case EmptyQueueBehavior::Sleep:
				{
					std::unique_lock<std::mutex> lock(tls.FailedQueuePopLock);
					
					// Check if we have a ready fiber
					// Lock
					while (tls.ReadFibersLock.test_and_set(std::memory_order_acquire)) {
						// Spin
					}
					// Prevent sleepy-time if we have ready fibers
					if (tls.ReadyFibers.empty()) {
						++tls.FailedQueuePopAttempts;
					}
					
					// Unlock
					tls.ReadFibersLock.clear(std::memory_order_release);

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

	
	// Start the quit sequence
	
	// Switch to the thread fibers
	ThreadLocalStorage &tls = taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];
	taskScheduler->m_fibers[tls.CurrentFiberIndex].SwitchToFiber(&tls.ThreadFiber);


	// We should never get here
	printf("Error: FiberStart should never return");
}

TaskScheduler::TaskScheduler()
	: m_numThreads(0),
	  m_fiberPoolSize(0), 
	  m_fibers(nullptr), 
	  m_freeFibers(nullptr), 
	  m_tls(nullptr) {
}

TaskScheduler::~TaskScheduler() {
	delete[] m_fibers;
	delete[] m_freeFibers;
	delete[] m_tls;
}

void TaskScheduler::Run(uint fiberPoolSize, TaskFunction mainTask, void *mainTaskArg, uint threadPoolSize, EmptyQueueBehavior behavior) {
	// Initialize the flags
	m_initialized.store(false, std::memory_order::memory_order_release);
	m_quit.store(false, std::memory_order_release);
	m_emptyQueueBehavior = behavior;

	// Create and populate the fiber pool
	m_fiberPoolSize = fiberPoolSize;
	m_fibers = new Fiber[fiberPoolSize];
	m_freeFibers = new std::atomic<bool>[fiberPoolSize];

	for (uint i = 0; i < fiberPoolSize; ++i) {
		m_fibers[i] = std::move(Fiber(512000, FiberStart, this));
		m_freeFibers[i].store(true, std::memory_order_release);
	}

	if (threadPoolSize == 0) {
		// 1 thread for each logical processor
		m_numThreads = GetNumHardwareThreads();
	} else {
		m_numThreads = threadPoolSize;
	}

	// Initialize threads and TLS
	m_threads.resize(m_numThreads);
	m_tls = new ThreadLocalStorage[m_numThreads];
	// Initialize all the locks before we start the other threads
	for (uint i = 0; i < m_numThreads; ++i) {
		m_tls[i].ReadFibersLock.clear();
	}

	// Set the properties for the current thread
	SetCurrentThreadAffinity(1);
	m_threads[0] = GetCurrentThread();

	// Create the remaining threads
	for (uint i = 1; i < m_numThreads; ++i) {
		ThreadStartArgs *threadArgs = new ThreadStartArgs();
		threadArgs->taskScheduler = this;
		threadArgs->threadIndex = i;

		if (!CreateThread(524288, ThreadStart, threadArgs, i, &m_threads[i])) {
			printf("Error: Failed to create all the worker threads");
			return;
		}
	}

	// Signal the worker threads that we're fully initialized
	m_initialized.store(true, std::memory_order_release);


	// Start the main task

	// Get a free fiber
	std::size_t freeFiberIndex = GetNextFreeFiberIndex();
	Fiber *freeFiber = &m_fibers[freeFiberIndex];

	// Repurpose it as the main task fiber and switch to it
	MainFiberStartArgs mainFiberArgs;
	mainFiberArgs.taskScheduler = this;
	mainFiberArgs.MainTask = mainTask;
	mainFiberArgs.Arg = mainTaskArg;

	freeFiber->Reset(MainFiberStart, &mainFiberArgs);
	m_tls[0].CurrentFiberIndex = freeFiberIndex;
	m_tls[0].ThreadFiber.SwitchToFiber(freeFiber);


	// And we're back
	// Wait for the worker threads to finish
	for (std::size_t i = 1; i < m_numThreads; ++i) {
		JoinThread(m_threads[i]);
	}

	// Cleanup
	delete[] m_fibers;
	m_fibers = nullptr;
	delete[] m_freeFibers;
	m_freeFibers = nullptr;
	delete[] m_tls;
	m_tls = nullptr;

	m_threads.clear();

	return;
}

void TaskScheduler::AddTask(Task task, AtomicCounter *counter) {
	if (counter != nullptr) {
		counter->Store(1);
	}

	const TaskBundle bundle = {task, counter};
	m_tls[GetCurrentThreadIndex()].TaskQueue.Push(bundle);

	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Find a thread that is sleeping and wake it
		for (uint i = 0; i < m_numThreads; ++i) {
			std::unique_lock<std::mutex> lock(m_tls[i].FailedQueuePopLock);
			if (m_tls[i].FailedQueuePopAttempts >= kFailedPopAttemptsHeuristic) {
				m_tls[i].FailedQueuePopAttempts = 0;
				m_tls[i].FailedQueuePopCV.notify_one();

				break;
			}
		}
	}
}

void TaskScheduler::AddTasks(uint numTasks, Task *tasks, AtomicCounter *counter) {
	if (counter != nullptr) {
		counter->Store(numTasks);
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	for (uint i = 0; i < numTasks; ++i) {
		const TaskBundle bundle = {tasks[i], counter};
		tls.TaskQueue.Push(bundle);
	}

	const EmptyQueueBehavior behavior = m_emptyQueueBehavior.load(std::memory_order_relaxed);
	if (behavior == EmptyQueueBehavior::Sleep) {
		// Wake all the threads
		for (uint i = 0; i < m_numThreads; ++i) {
			{
				std::unique_lock<std::mutex> lock(m_tls[i].FailedQueuePopLock);
				m_tls[i].FailedQueuePopAttempts = 0;
			}
			m_tls[i].FailedQueuePopCV.notify_all();
		}
	}
}

std::size_t TaskScheduler::GetCurrentThreadIndex() {
	#if defined(FTL_WIN32_THREADS)
		DWORD threadId = GetCurrentThreadId();
		for (std::size_t i = 0; i < m_numThreads; ++i) {
			if (m_threads[i].Id == threadId) {
				return i;
			}
		}
	#elif defined(FTL_POSIX_THREADS)
		pthread_t currentThread = pthread_self();
		for (std::size_t i = 0; i < m_numThreads; ++i) {
			if (pthread_equal(currentThread, m_threads[i])) {
				return i;
			}
		}
	#endif

	return FTL_INVALID_INDEX;
}

bool TaskScheduler::GetNextTask(TaskBundle *nextTask) {
	std::size_t currentThreadIndex = GetCurrentThreadIndex();
	ThreadLocalStorage &tls = m_tls[currentThreadIndex];

	// Try to pop from our own queue
	if (tls.TaskQueue.Pop(nextTask)) {
		return true;
	}

	// Ours is empty, try to steal from the others'
	const std::size_t threadIndex = tls.LastSuccessfulSteal;
	for (std::size_t i = 0; i < m_numThreads; ++i) {
		const std::size_t threadIndexToStealFrom = (threadIndex + i) % m_numThreads;
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

std::size_t TaskScheduler::GetNextFreeFiberIndex() {
	for (uint j = 0; ; ++j) {
		for (std::size_t i = 0; i < m_fiberPoolSize; ++i) {
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
	// The code structure guarantees that between any two fiber switches, the code will always end up in WaitForCounter or FiberStart.
	// Therefore, instead of using a helper fiber and immediately pushing the fiber to the fiber pool or waiting list,
	// we defer the push until the next fiber gets to one of those two places
	// 
	// Proof:
	// There are only two places where we switch fibers:
	// 1. When we're waiting for a counter, we pull a new fiber from the fiber pool and switch to it.
	// 2. When we found a counter that's ready, we put the current fiber back in the fiber pool, and switch to the waiting fiber.
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
		tls.OldFiberIndex = FTL_INVALID_INDEX;
		break;
	case FiberDestination::ToWaiting:
		// The waiting fibers are stored directly in their counters
		// They have an atomic<bool> that signals whether the waiting fiber can be consumed if it's ready
		// We just have to set it to true
		tls.OldFiberStoredFlag->store(true, std::memory_order_relaxed);
		tls.OldFiberDestination = FiberDestination::None;
		tls.OldFiberIndex = FTL_INVALID_INDEX;
		break;
	case FiberDestination::None:
	default:
		break;
	}
}

void TaskScheduler::AddReadyFiber(std::size_t pinnedThreadIndex, std::size_t fiberIndex, std::atomic<bool> *fiberStoredFlag) {
	ThreadLocalStorage *tls;
	if (pinnedThreadIndex == std::numeric_limits<std::size_t>::max()) {
		tls = &m_tls[GetCurrentThreadIndex()];
	} else {
		tls = &m_tls[pinnedThreadIndex];
	}

	// Lock
	while (tls->ReadFibersLock.test_and_set(std::memory_order_acquire)) {
		// Spin
	}

	tls->ReadyFibers.emplace_back(fiberIndex, fiberStoredFlag);

	// Unlock
	tls->ReadFibersLock.clear(std::memory_order_release);

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

void TaskScheduler::WaitForCounter(AtomicCounter *counter, uint value, bool pinToCurrentThread) {
	// Fast out
	if (counter->Load(std::memory_order_relaxed) == value) {
		return;
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	std::size_t currentFiberIndex = tls.CurrentFiberIndex;

	std::size_t pinnedThreadIndex;
	if (pinToCurrentThread) {
		pinnedThreadIndex = GetCurrentThreadIndex();
	} else {
		pinnedThreadIndex = std::numeric_limits<std::size_t>::max();
	}
	std::atomic<bool> *fiberStoredFlag = new std::atomic<bool>(false);
	bool alreadyDone = counter->AddFiberToWaitingList(tls.CurrentFiberIndex, value, fiberStoredFlag, pinnedThreadIndex);

	// The counter finished while we were trying to put it in the waiting list
	// Just clean up and trivially return
	if (alreadyDone) {
		delete fiberStoredFlag;
		return;
	}

	// Get a free fiber
	std::size_t freeFiberIndex = GetNextFreeFiberIndex();

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

