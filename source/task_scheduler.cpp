/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2017
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

#include "fiber_tasking_lib/task_scheduler.h"

#include "fiber_tasking_lib/atomic_counter.h"


namespace FiberTaskingLib {

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

	// Switch to the thread fibers
	ThreadLocalStorage &tls = taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];
	taskScheduler->m_fibers[tls.CurrentFiberIndex].SwitchToFiber(&tls.ThreadFiber);


	// We should never get here
	printf("Error: FiberStart should never return");
}

void TaskScheduler::FiberStart(void *arg) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	while (!taskScheduler->m_quit.load(std::memory_order_acquire)) {
		// Clean up from the last fiber to run on this thread
		taskScheduler->CleanUpOldFiber();

		// Check if there are any pinned fibers that are ready
		std::size_t waitingFiberIndex = FTL_INVALID_INDEX;
		ThreadLocalStorage &tls = taskScheduler->m_tls[taskScheduler->GetCurrentThreadIndex()];

		for (std::size_t i = 0; i < tls.PinnedTasks.size(); i++) {
			const PinnedWaitingFiberBundle *bundle = &tls.PinnedTasks[i];
			if (bundle->Counter->Load() == bundle->TargetValue) {
				waitingFiberIndex = bundle->FiberIndex;
				tls.PinnedTasks.erase(tls.PinnedTasks.begin() + i);
				
				break;
			}
		}

		// If there aren't any pinned fibers, check if there are any ready fibers
		if (waitingFiberIndex == FTL_INVALID_INDEX) {
			if (!tls.ReadyFibers.empty()) {
				waitingFiberIndex = tls.ReadyFibers.front();
				tls.ReadyFibers.pop();
			}
		}

		if (waitingFiberIndex != FTL_INVALID_INDEX) {
			// Found a waiting task that is ready to continue

			tls.OldFiberIndex = tls.CurrentFiberIndex;
			tls.CurrentFiberIndex = waitingFiberIndex;
			tls.OldFiberDestination = FiberDestination::ToPool;
			
			// Switch
			taskScheduler->m_fibers[tls.OldFiberIndex].SwitchToFiber(&taskScheduler->m_fibers[tls.CurrentFiberIndex]);

			// And we're back
		} else {
			// Get a new task from the queue, and execute it
			TaskBundle nextTask;
			if (!taskScheduler->GetNextTask(&nextTask)) {
				// Spin
			} else {
				nextTask.TaskToExecute.Function(taskScheduler, nextTask.TaskToExecute.ArgData);
				if (nextTask.Counter != nullptr) {
					nextTask.Counter->FetchSub(1);
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

void TaskScheduler::Run(uint fiberPoolSize, TaskFunction mainTask, void *mainTaskArg, uint threadPoolSize) {
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

	// Initialize all the things
	m_quit.store(false, std::memory_order_release);
	m_threads.resize(m_numThreads);
	m_tls = new ThreadLocalStorage[m_numThreads];

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

	return;
}

void TaskScheduler::AddTask(Task task, AtomicCounter *counter) {
	if (counter != nullptr) {
		counter->Store(1);
	}

	TaskBundle bundle = {task, counter};
	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	tls.TaskQueue.Push(bundle);
}

void TaskScheduler::AddTasks(uint numTasks, Task *tasks, AtomicCounter *counter) {
	if (counter != nullptr) {
		counter->Store(numTasks);
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	for (uint i = 0; i < numTasks; ++i) {
		TaskBundle bundle = {tasks[i], counter};
		tls.TaskQueue.Push(bundle);
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
	std::size_t threadIndex = tls.LastSuccessfulSteal;
	for (std::size_t i = 0; i < m_numThreads; ++i) {
		const std::size_t threadIndexToStealFrom = (threadIndex + i) % m_numThreads;
		if (threadIndexToStealFrom == currentThreadIndex) {
			continue;
		}
		ThreadLocalStorage &otherTLS = m_tls[threadIndexToStealFrom];
		if (otherTLS.TaskQueue.Steal(nextTask)) {
			tls.LastSuccessfulSteal = i;
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
	// The code structure guarantees that between any two fiber switches, the code will always end up in WaitForCounter or FibeStart.
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
	// The while and the if/else in FiberStart will guarantee the code will call CleanUpOldFiber() before executing any other fiber. 
	// QED
	// 
	// Case 2:
	// A waiting fiber can do two things:
	//    a. Finish the task and return
	//    b. Wait on another counter
	// In case a, the while loop and if/else will again guarantee the code will call CleanUpOldFiber() before executing any other fiber.
	// In case b, WaitOnCounter will directly call CleanUpOldFiber(). Any further code is just a recursion.
	// QED

	// In this specific implementation, the fiber pool and waiting list are flat arrays signaled by atomics
	// So in order to "Push" the fiber to the fiber pool or waiting list, we just set their corresponding atomics to true
	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	switch (tls.OldFiberDestination) {
	case FiberDestination::ToPool:
		m_freeFibers[tls.OldFiberIndex].store(true, std::memory_order_release);
		tls.OldFiberDestination = FiberDestination::None;
		tls.OldFiberIndex = FTL_INVALID_INDEX;
		break;
	case FiberDestination::ToWaiting:
		tls.OldFiberStoredFlag->store(true, std::memory_order_release);
		tls.OldFiberDestination = FiberDestination::None;
		tls.OldFiberIndex = FTL_INVALID_INDEX;
		break;
	case FiberDestination::None:
	default:
		break;
	}
}

void TaskScheduler::AddReadyFiber(std::size_t fiberIndex) {
	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	tls.ReadyFibers.push(fiberIndex);
}

void TaskScheduler::WaitForCounter(AtomicCounter *counter, uint value, bool pinToCurrentThread) {
	// Fast out
	if (counter->Load() == value) {
		return;
	}

	ThreadLocalStorage &tls = m_tls[GetCurrentThreadIndex()];
	std::size_t currentFiberIndex = tls.CurrentFiberIndex;

	// Get a free fiber
	std::size_t freeFiberIndex = GetNextFreeFiberIndex();

	// Clean up the old fiber
	CleanUpOldFiber();

	if (pinToCurrentThread) {
		// If task is pinned, put WaitingBundle in local array
		tls.PinnedTasks.emplace_back(currentFiberIndex, counter, value);
	} else {
		// If not pinned, ask the counter to track it
		std::atomic_bool *storedFlag = counter->AddFiberToWaitingList(tls.CurrentFiberIndex, value);
		// Early out if counter was satisfied
		if (!storedFlag) return;

		// Fill in tls
		tls.OldFiberIndex = currentFiberIndex;
		tls.CurrentFiberIndex = freeFiberIndex;
		tls.OldFiberDestination = FiberDestination::ToWaiting;
		tls.OldFiberStoredFlag = storedFlag;
	}

	// Switch
	m_fibers[currentFiberIndex].SwitchToFiber(&m_fibers[freeFiberIndex]);

	// And we're back
}

} // End of namespace FiberTaskingLib
