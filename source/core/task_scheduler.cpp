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

#include "fiber_tasking_lib/task_scheduler.h"

#include "fiber_tasking_lib/global_args.h"


namespace FiberTaskingLib {

TLS_VARIABLE(FiberType, tls_destFiber);
TLS_VARIABLE(FiberType, tls_originFiber);
TLS_VARIABLE(AtomicCounter *, tls_waitingCounter);
TLS_VARIABLE(int, tls_waitingValue);


struct ThreadStartArgs {
	GlobalArgs *globalArgs;
	uint threadIndex;
};

THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStart(void *arg) {
	ThreadStartArgs *threadArgs = static_cast<ThreadStartArgs *>(arg);
	GlobalArgs *globalArgs = threadArgs->globalArgs;

	FiberType threadFiber = FTLConvertThreadToFiber();
	FTLSetCurrentFiber(threadFiber);

	// Create switching fibers for this thread
	globalArgs->g_taskScheduler.m_waitingTaskLock.lock();
	globalArgs->g_taskScheduler.m_fiberSwitchingFibers[FTLGetCurrentThreadId()] = FTLCreateFiber(FTL_HELPER_FIBER_STACK_SIZE, FiberSwitchStart, reinterpret_cast<fiber_arg_t>(&globalArgs->g_taskScheduler));
	globalArgs->g_taskScheduler.m_counterWaitingFibers[FTLGetCurrentThreadId()] = FTLCreateFiber(FTL_HELPER_FIBER_STACK_SIZE, CounterWaitStart, reinterpret_cast<fiber_arg_t>(&globalArgs->g_taskScheduler));
	globalArgs->g_taskScheduler.m_waitingTaskLock.unlock();

	// Clean up
	delete threadArgs;

	FiberStart(reinterpret_cast<fiber_arg_t>(globalArgs));

	THREAD_FUNC_END;
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, FiberStart) {
	GlobalArgs *globalArgs = reinterpret_cast<GlobalArgs *>(arg);
	TaskScheduler *taskScheduler = &globalArgs->g_taskScheduler;

	while (!taskScheduler->m_quit.load()) {
		// Check if any of the waiting tasks are ready
		WaitingTask waitingTask;
		bool waitingTaskReady = false;

		taskScheduler->m_waitingTaskLock.lock();
		auto iter = taskScheduler->m_waitingTasks.begin();
		for (; iter != taskScheduler->m_waitingTasks.end(); ++iter) {
			if (iter->Counter->load() == iter->Value) {
				waitingTaskReady = true;
				break;
			}
		}
		if (waitingTaskReady) {
			waitingTask = *iter;

			// Optimization for removing an item from a vector as suggested by ryeguy on reddit
			// Explained here: http://stackoverflow.com/questions/4442477/remove-ith-item-from-c-stdvector/4442529#4442529
			// Essentially, rather than forcing a memcpy to shift all the remaining elements down after the erase,
			// we move the last element into the place where the erased element was. Then we pop off the last element

			// Check that we're not already the last item
			// Move assignment to self is not defined
			if (iter != (--taskScheduler->m_waitingTasks.end())) {
				*iter = std::move(taskScheduler->m_waitingTasks.back());
			}
			taskScheduler->m_waitingTasks.pop_back();

		}
		taskScheduler->m_waitingTaskLock.unlock();

		if (waitingTaskReady) {
			taskScheduler->SwitchFibers(waitingTask.Fiber);
		}


		TaskBundle nextTask;
		if (!taskScheduler->GetNextTask(&nextTask)) {
			std::this_thread::yield();
		} else {
			nextTask.TaskToExecute.Function(&globalArgs->g_taskScheduler, &globalArgs->g_heap, &globalArgs->g_allocator, nextTask.TaskToExecute.ArgData);
			nextTask.Counter->fetch_sub(1);
		}
	}

	FTLConvertFiberToThread(FTLGetCurrentFiber());
	globalArgs->g_taskScheduler.m_numActiveWorkerThreads.fetch_sub(1);
	FTLEndCurrentThread();
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, FiberSwitchStart) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);
	ThreadId threadId = FTLGetCurrentThreadId();

	while (true) {
		taskScheduler->m_fiberPool.enqueue(GetTLSData(tls_originFiber));
		FiberType destFiber = GetTLSData(tls_destFiber);

		FTLSetCurrentFiber(destFiber);
		FTLSwitchToFiber(taskScheduler->m_fiberSwitchingFibers[threadId], destFiber);
	}
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, CounterWaitStart) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);
	ThreadId threadId = FTLGetCurrentThreadId();

	while (true) {
		taskScheduler->m_waitingTaskLock.lock();
		taskScheduler->m_waitingTasks.emplace_back(GetTLSData(tls_originFiber), GetTLSData(tls_waitingCounter), GetTLSData(tls_waitingValue));
		taskScheduler->m_waitingTaskLock.unlock();

		FiberType destFiber = GetTLSData(tls_destFiber);

		FTLSetCurrentFiber(destFiber);
		FTLSwitchToFiber(taskScheduler->m_counterWaitingFibers[threadId], destFiber);
	}
}



TaskScheduler::TaskScheduler()
		: m_numThreads(0),
		  m_threads(nullptr) {
	m_quit.store(false);
}

TaskScheduler::~TaskScheduler() {
	delete[] m_threads;

	FiberType fiber;
	while (m_fiberPool.try_dequeue(fiber)) {
		FTLDeleteFiber(fiber);
	}

	for (auto iter = m_fiberSwitchingFibers.begin(); iter != m_fiberSwitchingFibers.end(); ++iter) {
		FTLDeleteFiber(iter->second);
	}
	for (auto iter = m_counterWaitingFibers.begin(); iter != m_counterWaitingFibers.end(); ++iter) {
		FTLDeleteFiber(iter->second);
	}
}

bool TaskScheduler::Initialize(uint fiberPoolSize, GlobalArgs *globalArgs) {
	for (uint i = 0; i < fiberPoolSize; ++i) {
		FiberType newFiber = FTLCreateFiber(524288, FiberStart, reinterpret_cast<fiber_arg_t>(globalArgs));
		m_fiberPool.enqueue(newFiber);
	}

	// Create an additional thread for each logical processor
	m_numThreads = FTLGetNumHardwareThreads();
	m_threads = new ThreadType[m_numThreads];
	m_numActiveWorkerThreads.store((uint)m_numThreads - 1);

	// Create switching fibers for this thread
	m_fiberSwitchingFibers[FTLGetCurrentThreadId()] = FTLCreateFiber(FTL_HELPER_FIBER_STACK_SIZE, FiberSwitchStart, reinterpret_cast<fiber_arg_t>(&globalArgs->g_taskScheduler));
	m_counterWaitingFibers[FTLGetCurrentThreadId()] = FTLCreateFiber(FTL_HELPER_FIBER_STACK_SIZE, CounterWaitStart, reinterpret_cast<fiber_arg_t>(&globalArgs->g_taskScheduler));

	// Set the affinity for the current thread and convert it to a fiber
	FTLSetCurrentThreadAffinity(1);
	m_threads[0] = FTLGetCurrentThread();
	FiberType mainThreadFiber = FTLConvertThreadToFiber();

	FTLSetCurrentFiber(mainThreadFiber);

	// Create the remaining threads
	for (uint i = 1; i < m_numThreads; ++i) {
		ThreadStartArgs *threadArgs = new ThreadStartArgs();
		threadArgs->globalArgs = globalArgs;
		threadArgs->threadIndex = i;

		ThreadType threadHandle;
		if (!FTLCreateThread(&threadHandle, 524288, ThreadStart, threadArgs, i)) {
			return false;
		}
		m_threads[i] = threadHandle;
	}

	return true;
}

std::shared_ptr<AtomicCounter> TaskScheduler::AddTask(Task task) {
	std::shared_ptr<AtomicCounter> counter(new AtomicCounter());
	counter->store(1);

	TaskBundle bundle = {task, counter};
	m_taskQueue.enqueue(bundle);

	return counter;
}

std::shared_ptr<AtomicCounter> TaskScheduler::AddTasks(uint numTasks, Task *tasks) {
	std::shared_ptr<AtomicCounter> counter(new AtomicCounter());
	counter->store(numTasks);

	for (uint i = 0; i < numTasks; ++i) {
		TaskBundle bundle = {tasks[i], counter};
		m_taskQueue.enqueue(bundle);
	}

	return counter;
}

bool TaskScheduler::GetNextTask(TaskBundle *nextTask) {
	bool success = m_taskQueue.try_dequeue(*nextTask);

	return success;
}

void TaskScheduler::SwitchFibers(FiberType fiberToSwitchTo) {
	FiberType currentFiber = FTLGetCurrentFiber();
	SetTLSData(tls_originFiber, currentFiber);
	SetTLSData(tls_destFiber, fiberToSwitchTo);

	FTLSwitchToFiber(currentFiber, m_fiberSwitchingFibers[FTLGetCurrentThreadId()]);
}

void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value) {
	if (counter->load() == value) {
		return;
	}

	// Switch to a new Fiber
	FiberType fiberToSwitchTo;
	m_fiberPool.wait_dequeue(fiberToSwitchTo);
	SetTLSData(tls_destFiber, fiberToSwitchTo);

	FiberType currentFiber = FTLGetCurrentFiber();
	SetTLSData(tls_originFiber, currentFiber);
	SetTLSData(tls_waitingCounter, counter.get());
	SetTLSData(tls_waitingValue, value);

	FTLSwitchToFiber(currentFiber, m_counterWaitingFibers[FTLGetCurrentThreadId()]);
}

void TaskScheduler::Quit() {
	m_quit.store(true);
	FTLConvertFiberToThread(FTLGetCurrentFiber());

	// Wait until all worker threads have finished
	//
	// We can't use traditional thread join mechanisms because we can't
	// guarantee the ThreadIds we started with are the ones we finish with
	// The 'physical' threads are always there, but the ThreadIds change
	// when we convert to/from fibers
	while (m_numActiveWorkerThreads.load() > 0) {
		std::this_thread::yield();
	}

	std::vector<ThreadType> threadsToCleanUp;
	ThreadType currentThread = FTLGetCurrentThread();
	for (uint i = 0; i < m_numThreads; ++i) {
		if (m_threads[i] != currentThread) {
			threadsToCleanUp.push_back(m_threads[i]);
		}
	}

	for (auto &thread : threadsToCleanUp) {
		FTLCleanupThread(thread);
	}
}

} // End of namespace FiberTaskingLib
