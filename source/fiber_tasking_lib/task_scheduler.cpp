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

#if defined(BOOST_CONTEXT)
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

	FiberId *tlsFake_destFiber;
	FiberId *tlsFake_originFiber;
	AtomicCounter **tlsFake_waitingCounter;
	int *tlsFake_waitingValue;
#else
	__declspec(thread) uint tls_threadId;

	__declspec(thread) void *tls_destFiber;
	__declspec(thread) void *tls_originFiber;
	__declspec(thread) AtomicCounter *tls_waitingCounter;
	__declspec(thread) int tls_waitingValue;
#endif


#if defined(BOOST_CONTEXT)
	// Boost.Context doesn't have a global way to get the current fiber
	FiberId *tlsFake_currentFiber;

	inline FiberId FTLGetCurrentFiber() {
		return GetTLSData(tlsFake_currentFiber);
	}
#else
	inline FiberId FTLGetCurrentFiber() {
		return GetCurrentFiber();
	}
#endif




struct ThreadStartArgs {
	GlobalArgs *globalArgs;
	uint threadId;
};

THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStart(void *arg) {
	ThreadStartArgs *threadArgs = (ThreadStartArgs *)arg;
	#if defined(BOOST_CONTEXT)
		g_threadIdToIndexMap[FTLGetCurrentThread()] = threadArgs->threadId;
	#else
		tls_threadId = threadArgs->threadId;
	#endif
	GlobalArgs *globalArgs = threadArgs->globalArgs;

	FiberId threadFiber = FTLConvertThreadToFiber();
	#if defined(BOOST_CONTEXT)
		tlsFake_currentFiber[threadArgs->threadId] = threadFiber;
	#endif

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

	FTLConvertFiberToThread();
	globalArgs->g_taskScheduler.m_numActiveWorkerThreads.fetch_sub(1);
	FTLEndCurrentThread();
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, FiberSwitchStart) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;
	#if defined(BOOST_CONTEXT)
		uint threadId = g_threadIdToIndexMap[FTLGetCurrentThread()];
	#endif

	while (true) {
		#if defined(BOOST_CONTEXT)
			taskScheduler->m_fiberPool.enqueue(GetTLSData(tlsFake_originFiber));
			FiberId destFiber = GetTLSData(tlsFake_destFiber);
			SetTLSData(tlsFake_currentFiber, destFiber);
			FTLSwitchToFiber(taskScheduler->m_fiberSwitchingFibers[threadId], destFiber);
		#else
			taskScheduler->m_fiberPool.enqueue(tls_originFiber);
			FTLSwitchToFiber(FTLGetCurrentFiber(), tls_destFiber);
		#endif
	}
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, CounterWaitStart) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;
	#if defined(BOOST_CONTEXT)
		uint threadId = g_threadIdToIndexMap[FTLGetCurrentThread()];
	#endif

	while (true) {
		#if defined(BOOST_CONTEXT)
			taskScheduler->m_waitingTaskLock.lock();
			taskScheduler->m_waitingTasks.emplace_back(GetTLSData(tlsFake_originFiber), GetTLSData(tlsFake_waitingCounter), GetTLSData(tlsFake_waitingValue));
			taskScheduler->m_waitingTaskLock.unlock();

			FiberId destFiber = GetTLSData(tlsFake_destFiber);
			SetTLSData(tlsFake_currentFiber, destFiber);
			FTLSwitchToFiber(taskScheduler->m_counterWaitingFibers[threadId], destFiber);
		#else
			taskScheduler->m_waitingTaskLock.lock();
			taskScheduler->m_waitingTasks.emplace_back(tls_originFiber, tls_waitingCounter, tls_waitingValue);
			taskScheduler->m_waitingTaskLock.unlock();

			FTLSwitchToFiber(FTLGetCurrentFiber(), tls_destFiber);
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

	#if defined(BOOST_CONTEXT)
		delete[] tlsFake_destFiber;
		delete[] tlsFake_originFiber;
		delete[] tlsFake_waitingCounter;
		delete[] tlsFake_waitingValue;
		delete[] tlsFake_currentFiber;
	#endif
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

	#if defined(BOOST_CONTEXT)
		tlsFake_destFiber = new FiberId[m_numThreads];
		tlsFake_originFiber = new FiberId[m_numThreads];
		tlsFake_waitingCounter = new AtomicCounter *[m_numThreads];
		tlsFake_waitingValue = new int[m_numThreads];
		tlsFake_currentFiber = new FiberId[m_numThreads];
	#endif


	// Create a switching fiber for each thread
	for (uint i = 0; i < m_numThreads; ++i) {
		m_fiberSwitchingFibers[i] = FTLCreateFiber(32768, FiberSwitchStart, (fiber_arg_t)&globalArgs->g_taskScheduler);
		m_counterWaitingFibers[i] = FTLCreateFiber(32768, CounterWaitStart, (fiber_arg_t)&globalArgs->g_taskScheduler);
	}

	// Set the affinity for the current thread and convert it to a fiber
	FTLSetCurrentThreadAffinity(1);
	m_threads[0] = FTLGetCurrentThread();
	FiberId mainThreadFiber = FTLConvertThreadToFiber();

	#if defined(BOOST_CONTEXT)
		g_threadIdToIndexMap[FTLGetCurrentThread()] = 0;
		tlsFake_currentFiber[0] = mainThreadFiber;
	#else
		tls_threadId = 0;		
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

void TaskScheduler::SwitchFibers(FiberId fiberToSwitchTo) {
	#if defined(BOOST_CONTEXT)
		SetTLSData(tlsFake_originFiber, FTLGetCurrentFiber());
		SetTLSData(tlsFake_destFiber, fiberToSwitchTo);

		FTLSwitchToFiber(FTLGetCurrentFiber(), m_fiberSwitchingFibers[g_threadIdToIndexMap[FTLGetCurrentThread()]]);
	#else
		tls_originFiber = FTLGetCurrentFiber();
		tls_destFiber = fiberToSwitchTo;

		FTLSwitchToFiber(FTLGetCurrentFiber(), m_fiberSwitchingFibers[tls_threadId]);
	#endif
}

void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value) {
	if (counter->load() == value) {
		return;
	}

	#if defined(BOOST_CONTEXT)
		// Switch to a new Fiber
		FiberId fiberToSwitchTo;
		m_fiberPool.wait_dequeue(fiberToSwitchTo);
		SetTLSData(tlsFake_destFiber, fiberToSwitchTo);

		SetTLSData(tlsFake_originFiber, FTLGetCurrentFiber());
		SetTLSData(tlsFake_waitingCounter, counter.get());
		SetTLSData(tlsFake_waitingValue, value);

		FTLSwitchToFiber(FTLGetCurrentFiber(), m_counterWaitingFibers[g_threadIdToIndexMap[FTLGetCurrentThread()]]);
	#else
		// Switch to a new Fiber
		m_fiberPool.wait_dequeue(tls_destFiber);

		tls_originFiber = FTLGetCurrentFiber();
		tls_waitingCounter = counter.get();
		tls_waitingValue = value;

		FTLSwitchToFiber(FTLGetCurrentFiber(), m_counterWaitingFibers[tls_threadId]);
	#endif
}

void TaskScheduler::Quit() {
	m_quit.store(true);
	FTLConvertFiberToThread();

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
