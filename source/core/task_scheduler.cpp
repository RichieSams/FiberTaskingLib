/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2016
 */

#include "fiber_tasking_lib/task_scheduler.h"

#include <thread>
#include <sstream>
#include <fstream>
#include <cstdarg>


namespace FiberTaskingLib {

TLS_VARIABLE(FiberType, tls_destFiber);
TLS_VARIABLE(FiberType, tls_originFiber);
TLS_VARIABLE(AtomicCounter *, tls_waitingCounter);
TLS_VARIABLE(int, tls_waitingValue);
TLS_VARIABLE(uint, tls_threadIndex);
TLS_VARIABLE(FiberType, tls_threadFibers);


void TaskScheduler::Log(const char *format, ...) {
	va_list args;
	va_start(args, format);
	
	char buffer[256];
	snprintf(buffer, 256, "Thread: [%u] ", GetTLSData(tls_threadIndex));
	std::string threadIndex(buffer);

	snprintf(buffer, 256, "Fiber: [%p] ", FTLGetCurrentFiber());
	std::string fiberNumber(buffer);

	vsnprintf(buffer, 256, format, args);
	std::string logLine(buffer);
	m_log[GetTLSData(tls_threadIndex)].push_back(threadIndex + fiberNumber + logLine);

	va_end(args);
}

//void TaskScheduler::Log(const char *format, ...) {}

void TaskScheduler::DumpLogToFile() {
	for (uint i = 0; i < m_log.size(); ++i) {
		std::stringstream fileName;
		fileName << "log" << i << ".txt";
		
		std::ofstream file;
		file.open(fileName.str());
		for (auto &line : m_log[i]) {
			file << line << "\n";
		}
		file.close();
	}
}

struct ThreadStartArgs {
	TaskScheduler *taskScheduler;
	uint threadIndex;
};

THREAD_FUNC_RETURN_TYPE TaskScheduler::ThreadStart(void *arg) {
	ThreadStartArgs *threadArgs = static_cast<ThreadStartArgs *>(arg);
	TaskScheduler *taskScheduler = threadArgs->taskScheduler;

	FiberType threadFiber = FTLConvertThreadToFiber();
	SetTLSData(tls_threadFibers, threadFiber);
	SetTLSData(tls_threadIndex, threadArgs->threadIndex);

	// Clean up
	delete threadArgs;

	taskScheduler->Log("Thread started", GetTLSData(tls_threadIndex));

	// Get a new fiber to switch to
	FiberType fiberToSwitchTo;
	while (!taskScheduler->m_fiberPool.Pop(&fiberToSwitchTo)) {
		// Spin
		taskScheduler->Log("Waiting on a fiber to start");
	}
	
	// Switch to it
	taskScheduler->Log("Switching to first fiber");
	FTLSwitchToFiber(threadFiber, fiberToSwitchTo);


	// And we've returned
	taskScheduler->Log("Thread shutting down");

	taskScheduler->m_threadsFinishedQuitting.fetch_add(1u);
	FTLEndCurrentThread();
	THREAD_FUNC_END;
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, FiberStart) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

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
			taskScheduler->Log("Found a waiting task that is ready to continue. Switching to it");
			taskScheduler->SwitchFibers(waitingTask.Fiber);
		}


		TaskBundle nextTask;
		if (!taskScheduler->GetNextTask(&nextTask)) {
			std::this_thread::yield();
		} else {
			taskScheduler->Log("Executing a new task from the queue");
			nextTask.TaskToExecute.Function(taskScheduler, nextTask.TaskToExecute.ArgData);
			nextTask.Counter->fetch_sub(1);
		}
	}

	
	// Start the quit sequence
	taskScheduler->Log("Switching to quit fibers");
	FiberType currentFiber = FTLGetCurrentFiber();
	FTLSwitchToFiber(currentFiber, taskScheduler->m_quitFibers[GetTLSData(tls_threadIndex)]);

	// We should never get here
	printf("Error: FiberStart should never return");
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, FiberSwitchStart) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	while (true) {
		taskScheduler->m_fiberPool.Push(GetTLSData(tls_originFiber));
		FiberType destFiber = GetTLSData(tls_destFiber);

		FTLSwitchToFiber(taskScheduler->m_fiberSwitchingFibers[GetTLSData(tls_threadIndex)], destFiber);
	}
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, CounterWaitStart) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	while (true) {
		taskScheduler->m_waitingTaskLock.lock();
		taskScheduler->m_waitingTasks.emplace_back(GetTLSData(tls_originFiber), GetTLSData(tls_waitingCounter), GetTLSData(tls_waitingValue));
		taskScheduler->m_waitingTaskLock.unlock();

		FiberType destFiber = GetTLSData(tls_destFiber);

		FTLSwitchToFiber(taskScheduler->m_counterWaitingFibers[GetTLSData(tls_threadIndex)], destFiber);
	}
}

FIBER_START_FUNCTION_CLASS_IMPL(TaskScheduler, QuitStart) {
	TaskScheduler *taskScheduler = reinterpret_cast<TaskScheduler *>(arg);

	taskScheduler->Log("Quit fiber started. Waiting for others");

	// Wait for the other threads to finish switching
	taskScheduler->m_threadsRemainingToQuit.fetch_sub(1u);
	while(taskScheduler->m_threadsRemainingToQuit.load() != 0u) {
		// Spin
	}
	
	taskScheduler->Log("Switching to threadFiber");
	FiberType destFiber = GetTLSData(tls_threadFibers);
	FTLSwitchToFiber(taskScheduler->m_quitFibers[GetTLSData(tls_threadIndex)], destFiber);
	
	// We should never get here
	printf("Error: QuitStart should never return");
}


TaskScheduler::TaskScheduler()
		: m_numThreads(0),
		  m_threads(nullptr) {
	m_quit.store(false);
}

TaskScheduler::~TaskScheduler() {
	delete[] m_threads;

	FiberType temp;
	while (m_fiberPool.Pop(&temp)) {
		FTLDeleteFiber(temp);
	}

	for (auto &fiber : m_fiberSwitchingFibers) {
		FTLDeleteFiber(fiber);
	}
	for (auto &fiber : m_counterWaitingFibers) {
		FTLDeleteFiber(fiber);
	}
	for (auto &fiber : m_quitFibers) {
		FTLDeleteFiber(fiber);
	}
}

bool TaskScheduler::Initialize(uint fiberPoolSize) {
	for (uint i = 0; i < fiberPoolSize; ++i) {
		FiberType newFiber = FTLCreateFiber(524288, FiberStart, reinterpret_cast<fiber_arg_t>(this));
		m_fiberPool.Push(newFiber);
	}

	// 1 thread for each logical processor
	m_numThreads = FTLGetNumHardwareThreads();
	m_threads = new ThreadType[m_numThreads];
	m_threadsRemainingToQuit.store(static_cast<uint>(m_numThreads));
	m_threadsFinishedQuitting.store(0u);
	m_fiberSwitchingFibers.resize(m_numThreads);
	m_counterWaitingFibers.resize(m_numThreads);
	m_quitFibers.resize(m_numThreads);
	m_log.resize(m_numThreads);

	// Create switching fibers for all the threads
	for (uint i = 0; i < m_numThreads; ++i) {
		m_fiberSwitchingFibers[i] = FTLCreateFiber(FTL_FIBER_MIN_STACK_SIZE, FiberSwitchStart, reinterpret_cast<fiber_arg_t>(this));
		m_counterWaitingFibers[i] = FTLCreateFiber(FTL_FIBER_MIN_STACK_SIZE, CounterWaitStart, reinterpret_cast<fiber_arg_t>(this));
		m_quitFibers[i] = FTLCreateFiber(FTL_FIBER_MIN_STACK_SIZE, QuitStart, reinterpret_cast<fiber_arg_t>(this));
	}

	// Set the affinity for the current thread and convert it to a fiber
	FTLSetCurrentThreadAffinity(1);
	m_threads[0] = FTLGetCurrentThread();
	FiberType mainThreadFiber = FTLConvertThreadToFiber();
	SetTLSData(tls_threadFibers, mainThreadFiber);
	SetTLSData(tls_threadIndex, 0u);

	// Create the remaining threads
	for (uint i = 1; i < m_numThreads; ++i) {
		ThreadStartArgs *threadArgs = new ThreadStartArgs();
		threadArgs->taskScheduler = this;
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
	m_taskQueue.Push(bundle);

	return counter;
}

std::shared_ptr<AtomicCounter> TaskScheduler::AddTasks(uint numTasks, Task *tasks) {
	std::shared_ptr<AtomicCounter> counter(new AtomicCounter());
	counter->store(numTasks);

	for (uint i = 0; i < numTasks; ++i) {
		TaskBundle bundle = {tasks[i], counter};
		m_taskQueue.Push(bundle);
	}

	return counter;
}

bool TaskScheduler::GetNextTask(TaskBundle *nextTask) {
	bool success = m_taskQueue.Pop(nextTask);

	return success;
}

void TaskScheduler::SwitchFibers(FiberType fiberToSwitchTo) {
	FiberType currentFiber = FTLGetCurrentFiber();
	SetTLSData(tls_originFiber, currentFiber);
	SetTLSData(tls_destFiber, fiberToSwitchTo);

	FTLSwitchToFiber(currentFiber, m_fiberSwitchingFibers[GetTLSData(tls_threadIndex)]);
}

void TaskScheduler::WaitForCounter(std::shared_ptr<AtomicCounter> &counter, int value) {
	if (counter->load() == value) {
		return;
	}

	// Get a new fiber to switch to
	FiberType fiberToSwitchTo;
	while(!m_fiberPool.Pop(&fiberToSwitchTo)) {
		// Spin
		Log("Waiting for a free fiber in the pool");
	}

	SetTLSData(tls_destFiber, fiberToSwitchTo);

	FiberType currentFiber = FTLGetCurrentFiber();
	SetTLSData(tls_originFiber, currentFiber);
	SetTLSData(tls_waitingCounter, counter.get());
	SetTLSData(tls_waitingValue, value);

	Log("Waiting for counter %ull. Switching to new fiber while we wait", counter.get());
	FTLSwitchToFiber(currentFiber, m_counterWaitingFibers[GetTLSData(tls_threadIndex)]);
	Log("Counter %ull is ready. Fiber returned", counter.get());
}

void TaskScheduler::Quit() {
	m_quit.store(true);
	
	Log("Quit() called");
	Log("Switching to quit fibers");

	// Switch to the quit fibers and wait for the other threads to follow suit
	FTLSwitchToFiber(FTLGetCurrentFiber(), m_quitFibers[GetTLSData(tls_threadIndex)]);

	// And we've returned
	Log("Returned from quit fibers");

	// Wait for all the threads to finish
	while (m_threadsFinishedQuitting.load() != m_numThreads - 1) {
		// Spin
	}
	

	DumpLogToFile();
}

} // End of namespace FiberTaskingLib