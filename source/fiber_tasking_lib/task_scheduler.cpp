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

#include <process.h>


namespace FiberTaskingLib {

__declspec(thread) void *tls_fiberToSwitchTo;
__declspec(thread) void *tls_currentFiber;
__declspec(thread) AtomicCounter *tls_waitingCounter;
__declspec(thread) int tls_waitingValue;

uint TaskScheduler::ThreadStart(void *arg) {
	ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
	FiberStart(arg);

	ConvertFiberToThread();
	return 1;
}

void TaskScheduler::FiberStart(void *arg) {
	GlobalArgs *globalArgs = (GlobalArgs *)arg;
	TaskScheduler *taskScheduler = &globalArgs->TaskScheduler;

	while (!taskScheduler->m_quit.load()) {
		// Check if any of the waiting tasks are ready
		WaitingTask waitingTask;
		bool waitingTaskReady = false;

		EnterCriticalSection(&taskScheduler->m_waitingTaskLock);
		auto iter = taskScheduler->m_waitingTasks.begin();
		for ( ; iter != taskScheduler->m_waitingTasks.end(); ++iter) {
			if (iter->Counter->load() == iter->Value) {
				waitingTaskReady = true;
				break;
			}
		}
		if (waitingTaskReady) {
			waitingTask = *iter;
			taskScheduler->m_waitingTasks.erase(iter);
		}
		LeaveCriticalSection(&taskScheduler->m_waitingTaskLock);

		if (waitingTaskReady) {
			taskScheduler->SwitchFibers(waitingTask.Fiber);
		}


		TaskBundle nextTask;
		if (!taskScheduler->GetNextTask(&nextTask)) {
			Sleep(1);
		} else {
			nextTask.Task.Function(&globalArgs->TaskScheduler, nextTask.Task.ArgData);
			nextTask.Counter->fetch_sub(1);
		}
	}
}

void __stdcall TaskScheduler::FiberSwitchStart(void *arg) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;

	while (true) {
		taskScheduler->m_fiberPool.enqueue(tls_currentFiber);
		SwitchToFiber(tls_fiberToSwitchTo);
	}
}

void __stdcall TaskScheduler::CounterWaitStart(void *arg) {
	TaskScheduler *taskScheduler = (TaskScheduler *)arg;

	while (true) {
		EnterCriticalSection(&taskScheduler->m_waitingTaskLock);
		taskScheduler->m_waitingTasks.emplace_back(tls_currentFiber, tls_waitingCounter, tls_waitingValue);
		LeaveCriticalSection(&taskScheduler->m_waitingTaskLock);

		SwitchToFiber(tls_fiberToSwitchTo);
	}
}



TaskScheduler::TaskScheduler()
		: m_numThreads(0),
		  m_threads(nullptr) {
	m_quit.store(false, std::memory_order_relaxed);
	InitializeCriticalSection(&m_waitingTaskLock);
}

TaskScheduler::~TaskScheduler() {
	delete[] m_threads;

	void *fiber;
	while (m_fiberPool.try_dequeue(fiber)) {
		DeleteFiber(fiber);
	}

	DeleteCriticalSection(&m_waitingTaskLock);
}

void TaskScheduler::Initialize(GlobalArgs *globalArgs) {
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);

	// Create an additional thread for each logical processor
	m_numThreads = sysinfo.dwNumberOfProcessors;
	m_threads = new HANDLE[m_numThreads];

	m_threads[0] = GetCurrentThread();
	for (DWORD i = 1; i < m_numThreads; ++i) {
		HANDLE threadHandle = (HANDLE)_beginthreadex(nullptr, 524288, ThreadStart, globalArgs, CREATE_SUSPENDED, nullptr);
		m_threads[i] = threadHandle;

		DWORD_PTR mask = 1 << i;
		SetThreadAffinityMask(threadHandle, mask);
		ResumeThread(threadHandle);
	}


	for (uint i = sysinfo.dwNumberOfProcessors - 1; i < FIBER_POOL_SIZE; ++i) {
		m_fiberPool.enqueue(CreateFiberEx(524288, 0, FIBER_FLAG_FLOAT_SWITCH, FiberStart, globalArgs));
	}

	m_fiberSwitchingFiber = CreateFiberEx(32768, 0, FIBER_FLAG_FLOAT_SWITCH, FiberSwitchStart, &globalArgs->TaskScheduler);
	m_counterWaitingFiber = CreateFiberEx(32768, 0, FIBER_FLAG_FLOAT_SWITCH, CounterWaitStart, &globalArgs->TaskScheduler);

	// Set the affinity for the current thread and convert it to a fiber
	SetThreadAffinityMask(GetCurrentThread(), 1);
	ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
}

std::shared_ptr<AtomicCounter> TaskScheduler::AddTask(Task task) {
	std::shared_ptr<AtomicCounter> counter(new AtomicCounter());
	counter->store(1, std::memory_order_relaxed);

	TaskBundle bundle = {task, counter};
	m_taskQueue.enqueue(bundle);

	return counter;
}

std::shared_ptr<AtomicCounter> TaskScheduler::AddTasks(uint numTasks, Task *tasks) {
	std::shared_ptr<AtomicCounter> counter(new AtomicCounter());
	counter->store(numTasks, std::memory_order_relaxed);

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
	tls_currentFiber = GetCurrentFiber();
	tls_fiberToSwitchTo = fiberToSwitchTo;

	SwitchToFiber(m_fiberSwitchingFiber);
}

void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value) {
	if (counter->load() == value) {
		return;
	}

	// Switch to a new Fiber
	m_fiberPool.wait_dequeue(tls_fiberToSwitchTo);

	tls_currentFiber = GetCurrentFiber();
	tls_waitingCounter = counter.get();
	tls_waitingValue = value;
	
	SwitchToFiber(m_counterWaitingFiber);
}

void TaskScheduler::Quit() {
	m_quit.store(true, std::memory_order_relaxed);
	ConvertFiberToThread();

	std::vector<HANDLE> workerThreads;
	for (uint i = 0; i < m_numThreads; ++i) {
		if (m_threads != GetCurrentThread()) {
			workerThreads.push_back(m_threads[i]);
		}
	}

	DWORD result = WaitForMultipleObjects(workerThreads.size(), &workerThreads[0], true, INFINITE);

	for (auto &workerThread : workerThreads) {
		CloseHandle(workerThread);
	}
}

} // End of namespace FiberTaskingLib
