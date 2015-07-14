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

TLS_VARIABLE(FiberId, tls_destFiber);
TLS_VARIABLE(FiberId, tls_originFiber);
TLS_VARIABLE(AtomicCounter *, tls_waitingCounter);
TLS_VARIABLE(int, tls_waitingValue);


struct ThreadStartArgs {
	GlobalArgs *globalArgs;
	uint threadIndex;
};

THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStart(void *arg) {
	ThreadStartArgs *threadArgs = (ThreadStartArgs *)arg;

	SetThreadIndex(threadArgs->threadIndex);
	GlobalArgs *globalArgs = threadArgs->globalArgs;

	FiberId threadFiber = FTLConvertThreadToFiber();
	FTLSetCurrentFiber(threadFiber);

	// Clean up
	delete threadArgs;

	FiberStart((fiber_arg_t)globalArgs);

	THREAD_FUNC_END;
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, FiberStart) {
	GlobalArgs *globalArgs = (GlobalArgs *)arg;
	TaskScheduler *taskScheduler = &globalArgs->g_taskScheduler;

	while (!taskScheduler->m_quit.load()) {
		// Check if any of the waiting tasks are ready
		WaitingTask waitingTask;
		bool waitingTaskReady = false;

		taskScheduler->m_waitingTaskLock.lock();
		auto iter = taskScheduler->m_waitingTasks.begin();
		for ( ; iter != taskScheduler->m_waitingTasks.end(); ++iter) {
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
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;
	uint threadId = GetThreadIndex();

	while (true) {
		taskScheduler->m_fiberPool.enqueue(GetTLSData(tls_originFiber));
		FiberId destFiber = GetTLSData(tls_destFiber);

		FTLSetCurrentFiber(destFiber);
		FTLSwitchToFiber(taskScheduler->m_fiberSwitchingFibers[threadId], destFiber);
	}
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, CounterWaitStart) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;
	uint threadId = GetThreadIndex();

	while (true) {
		taskScheduler->m_waitingTaskLock.lock();
		taskScheduler->m_waitingTasks.emplace_back(GetTLSData(tls_originFiber), GetTLSData(tls_waitingCounter), GetTLSData(tls_waitingValue));
		taskScheduler->m_waitingTaskLock.unlock();

		FiberId destFiber = GetTLSData(tls_destFiber);

		FTLSetCurrentFiber(destFiber);
		FTLSwitchToFiber(taskScheduler->m_counterWaitingFibers[threadId], destFiber);
	}
}



TaskScheduler::TaskScheduler()
		: m_numThreads(0),
		  m_threads(nullptr),
		  m_fiberSwitchingFibers(nullptr),
		  m_counterWaitingFibers(nullptr) {
	m_quit.store(false);
}

TaskScheduler::~TaskScheduler() {
	delete[] m_threads;

	FiberId fiber;
	while (m_fiberPool.try_dequeue(fiber)) {
		FTLDeleteFiber(fiber);
	}

	for (uint i = 0; i < m_numThreads; ++i) {
		FTLDeleteFiber(m_fiberSwitchingFibers[i]);
		FTLDeleteFiber(m_counterWaitingFibers[i]);
	}
	delete[] m_fiberSwitchingFibers;
	delete[] m_counterWaitingFibers;

	DestroyTLSVariable(tls_destFiber);
	DestroyTLSVariable(tls_originFiber);
	DestroyTLSVariable(tls_waitingCounter);
	DestroyTLSVariable(tls_waitingValue);

	FTLDestroyCurrentFiber();
}

bool TaskScheduler::Initialize(uint fiberPoolSize, GlobalArgs *globalArgs) {
	for (uint i = 0; i < fiberPoolSize; ++i) {
		FiberId newFiber = FTLCreateFiber(524288, FiberStart, (fiber_arg_t)globalArgs);
		m_fiberPool.enqueue(newFiber);
	}

	// Create an additional thread for each logical processor
	m_numThreads = FTLGetNumHardwareThreads();
	m_threads = new ThreadId[m_numThreads];
	m_numActiveWorkerThreads.store((uint)m_numThreads - 1);
	m_fiberSwitchingFibers = new FiberId[m_numThreads];
	m_counterWaitingFibers = new FiberId[m_numThreads];

	CreateTLSVariable(&tls_destFiber, m_numThreads);
	CreateTLSVariable(&tls_originFiber, m_numThreads);
	CreateTLSVariable(&tls_waitingCounter, m_numThreads);
	CreateTLSVariable(&tls_waitingValue, m_numThreads);

    FTLInitializeCurrentFiber(m_numThreads);


	// Create a switching fiber for each thread
	for (uint i = 0; i < m_numThreads; ++i) {
		m_fiberSwitchingFibers[i] = FTLCreateFiber(32768, FiberSwitchStart, (fiber_arg_t)&globalArgs->g_taskScheduler);
		m_counterWaitingFibers[i] = FTLCreateFiber(32768, CounterWaitStart, (fiber_arg_t)&globalArgs->g_taskScheduler);
	}

	// Set the affinity for the current thread and convert it to a fiber
	FTLSetCurrentThreadAffinity(1);
	m_threads[0] = FTLGetCurrentThread();
	FiberId mainThreadFiber = FTLConvertThreadToFiber();

	SetThreadIndex(0);
	FTLSetCurrentFiber(mainThreadFiber);
	
	// Create the remaining threads
	for (uint i = 1; i < m_numThreads; ++i) {
		ThreadStartArgs *threadArgs = new ThreadStartArgs();
		threadArgs->globalArgs = globalArgs;
		threadArgs->threadIndex = i;

		ThreadId threadHandle;
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

void TaskScheduler::SwitchFibers(FiberId fiberToSwitchTo) {
    FiberId currentFiber = FTLGetCurrentFiber();
	SetTLSData(tls_originFiber, currentFiber);
	SetTLSData(tls_destFiber, fiberToSwitchTo);

	FTLSwitchToFiber(currentFiber, m_fiberSwitchingFibers[GetThreadIndex()]);
}

void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value) {
	if (counter->load() == value) {
		return;
	}

	// Switch to a new Fiber
	FiberId fiberToSwitchTo;
	m_fiberPool.wait_dequeue(fiberToSwitchTo);
	SetTLSData(tls_destFiber, fiberToSwitchTo);

    FiberId currentFiber = FTLGetCurrentFiber();
	SetTLSData(tls_originFiber, currentFiber);
	SetTLSData(tls_waitingCounter, counter.get());
	SetTLSData(tls_waitingValue, value);

	FTLSwitchToFiber(currentFiber, m_counterWaitingFibers[GetThreadIndex()]);
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

	std::vector<ThreadId> threadsToCleanUp;
	ThreadId currentThread = FTLGetCurrentThread();
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
