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
#include "fiber_tasking_lib/fiber_abstraction.h"


namespace FiberTaskingLib {


#if defined(_MSC_VER)
	__declspec(thread) uint tls_threadId;

	__declspec(thread) void *tls_fiberToSwitchTo;
	__declspec(thread) void *tls_currentFiber;
	__declspec(thread) AtomicCounter *tls_waitingCounter;
	__declspec(thread) int tls_waitingValue;
#else
	// We can't use thread local storage with Boost.Context
	// so we have to fake it
	#include<unordered_map>

	std::unordered_map<ThreadId, uint> g_threadIdToIndexMap;

	template<class T>
	inline T GetTLSData(T *tlsFakeArray) {
		return tlsFakeArray[g_threadIdToIndexMap[FTLGetCurrentThread()]];
	}

	template<class T>
	inline void SetTLSData(T *tlsFakeArray, T value) {
		tlsFakeArray[g_threadIdToIndexMap[FTLGetCurrentThread()]] = value;
	}

	void **tlsFake_fiberToSwitchTo;
	void **tlsFake_currentFiber;
	AtomicCounter **tlsFake_waitingCounter;
	int *tlsFake_waitingValue;
#endif


struct ThreadStartArgs {
	GlobalArgs *globalArgs;
	uint threadId;
};

THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStart(void *arg) {
	ThreadStartArgs *threadArgs = (ThreadStartArgs *)arg;
	#if defined(_MSC_VER)
		tls_threadId = threadArgs->threadId;
	#else
		g_threadIdToIndexMap[FTLGetCurrentThread()] = threadArgs->threadId;
	#endif
	GlobalArgs *globalArgs = threadArgs->globalArgs;

	// Clean up
	delete threadArgs;

	FTLConvertThreadToFiber();
	FiberStart(globalArgs);

	FTLConvertFiberToThread();

	THREAD_FUNC_END;
}

void TaskScheduler::FiberStart(void *arg) {
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
}

void STDCALL TaskScheduler::FiberSwitchStart(void *arg) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;

	while (true) {
		#if defined(_MSC_VER)
			taskScheduler->m_fiberPool.enqueue(tls_currentFiber);
			FTLSwitchToFiber(tls_fiberToSwitchTo);
		#else
			taskScheduler->m_fiberPool.enqueue(GetTLSData(tlsFake_currentFiber));
			FTLSwitchToFiber(GetTLSData(tlsFake_fiberToSwitchTo));
		#endif
	}
}

void STDCALL TaskScheduler::CounterWaitStart(void *arg) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;

	while (true) {
		#if defined(_MSC_VER)
			taskScheduler->m_waitingTaskLock.lock();
			taskScheduler->m_waitingTasks.emplace_back(tls_currentFiber, tls_waitingCounter, tls_waitingValue);
			taskScheduler->m_waitingTaskLock.unlock();

			FTLSwitchToFiber(tls_fiberToSwitchTo);
		#else
			taskScheduler->m_waitingTaskLock.lock();
			taskScheduler->m_waitingTasks.emplace_back(GetTLSData(tlsFake_currentFiber), GetTLSData(tlsFake_waitingCounter), GetTLSData(tlsFake_waitingValue));
			taskScheduler->m_waitingTaskLock.unlock();

			FTLSwitchToFiber(GetTLSData(tlsFake_fiberToSwitchTo));
		#endif
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

	void *fiber;
	while (m_fiberPool.try_dequeue(fiber)) {
		FTLDeleteFiber(fiber);
	}

	for (uint i = 0; i < m_numThreads; ++i) {
		FTLDeleteFiber(m_fiberSwitchingFibers[i]);
		FTLDeleteFiber(m_counterWaitingFibers[i]);
	}
	delete[] m_fiberSwitchingFibers;
	delete[] m_counterWaitingFibers;
}

bool TaskScheduler::Initialize(uint fiberPoolSize, GlobalArgs *globalArgs) {
	for (uint i = 0; i < fiberPoolSize; ++i) {
		m_fiberPool.enqueue(FTLCreateFiber(524288, FiberStart, globalArgs));
	}

	// Create an additional thread for each logical processor
	m_numThreads = FTLGetNumHardwareThreads();
	m_threads = new ThreadId[m_numThreads];
	m_fiberSwitchingFibers = new void *[m_numThreads];
	m_counterWaitingFibers = new void *[m_numThreads];


	// Create a switching fiber for each thread
	for (uint i = 0; i < m_numThreads; ++i) {
		m_fiberSwitchingFibers[i] = FTLCreateFiber(32768, FiberSwitchStart, &globalArgs->g_taskScheduler);
		m_counterWaitingFibers[i] = FTLCreateFiber(32768, CounterWaitStart, &globalArgs->g_taskScheduler);
	}

	// Set the affinity for the current thread and convert it to a fiber
	FTLSetCurrentThreadAffinity(1);
	FTLConvertThreadToFiber();
	m_threads[0] = FTLGetCurrentThread();
	#if defined(_MSC_VER)
		tls_threadId = 0;
	#else
		g_threadIdToIndexMap[FTLGetCurrentThread()] = 0;
	#endif
	
	// Create the remaining threads
	for (uint i = 1; i < m_numThreads; ++i) {
		ThreadStartArgs *threadArgs = new ThreadStartArgs();
		threadArgs->globalArgs = globalArgs;
		threadArgs->threadId = i;

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

void TaskScheduler::SwitchFibers(void *fiberToSwitchTo) {
	#if defined(_MSC_VER)
		tls_currentFiber = FTLGetCurrentFiber();
		tls_fiberToSwitchTo = fiberToSwitchTo;

		FTLSwitchToFiber(m_fiberSwitchingFibers[tls_threadId]);
	#else
		SetTLSData(tlsFake_currentFiber, FTLGetCurrentFiber());
		SetTLSData(tlsFake_fiberToSwitchTo, fiberToSwitchTo);

		FTLSwitchToFiber(m_fiberSwitchingFibers[g_threadIdToIndexMap[FTLGetCurrentThread()]]);
	#endif
}

void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value) {
	if (counter->load() == value) {
		return;
	}

	#if defined(_MSC_VER)
		// Switch to a new Fiber
		m_fiberPool.wait_dequeue(tls_fiberToSwitchTo);

		tls_currentFiber = FTLGetCurrentFiber();
		tls_waitingCounter = counter.get();
		tls_waitingValue = value;

		FTLSwitchToFiber(m_counterWaitingFibers[tls_threadId]);
	#else
		// Switch to a new Fiber
		void *fiberToSwitchTo;
		m_fiberPool.wait_dequeue(fiberToSwitchTo);
		SetTLSData(tlsFake_fiberToSwitchTo, fiberToSwitchTo);

		SetTLSData(tlsFake_currentFiber, FTLGetCurrentFiber());
		SetTLSData(tlsFake_waitingCounter, counter.get());
		SetTLSData(tlsFake_waitingValue, value);

		FTLSwitchToFiber(m_counterWaitingFibers[g_threadIdToIndexMap[FTLGetCurrentThread()]]);
	#endif
}

void TaskScheduler::Quit() {
	m_quit.store(true);
	FTLConvertFiberToThread();

	std::vector<ThreadId> workerThreads;
	for (uint i = 0; i < m_numThreads; ++i) {
		if (m_threads != FTLGetCurrentThread()) {
			workerThreads.push_back(m_threads[i]);
		}
	}

	FTLJoinThreads((uint)workerThreads.size(), &workerThreads[0]);

	for (auto &workerThread : workerThreads) {
		FTLTerminateThread(workerThread);
	}
}

} // End of namespace FiberTaskingLib
