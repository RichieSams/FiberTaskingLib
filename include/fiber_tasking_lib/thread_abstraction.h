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

#pragma once

#include "fiber_tasking_lib/typedefs.h"
#include "fiber_tasking_lib/config.h"

#include <cassert>


#if defined(FTL_WIN32_THREADS)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <process.h>
#include <atomic>


namespace FiberTaskingLib {

struct Win32Thread {
	HANDLE Handle;
	DWORD Id;
};

typedef Win32Thread ThreadType;

struct EventType {
	HANDLE event;
	std::atomic_ulong countWaiters;
};
const uint32 EVENTWAIT_INFINITE = INFINITE;

typedef uint(__stdcall *ThreadStartRoutine)(void *arg);
#define FTL_THREAD_FUNC_RETURN_TYPE uint
#define FTL_THREAD_FUNC_DECL FTL_THREAD_FUNC_RETURN_TYPE __stdcall
#define FTL_THREAD_FUNC_END return 0


/**
* Create a native thread
*
* @param stackSize       The size of the stack
* @param startRoutine    The start routine
* @param arg             The argument for the start routine
* @param coreAffinity    The core affinity
* @param returnThread    The handle for the newly created thread. Undefined if thread creation fails
*
* @return    True if thread creation succeeds, false if it fails
*/
inline bool CreateThread(uint stackSize, ThreadStartRoutine startRoutine, void *arg, ThreadType *returnThread) {
	HANDLE handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, startRoutine, arg, 0u, nullptr));
	returnThread->Handle = handle;
	returnThread->Id = GetThreadId(handle);

	return handle != nullptr;
}

/**
* Create a native thread
*
* @param stackSize       The size of the stack
* @param startRoutine    The start routine
* @param arg             The argument for the start routine
* @param coreAffinity    The core affinity
* @param returnThread    The handle for the newly created thread. Undefined if thread creation fails
*
* @return    True if thread creation succeeds, false if it fails
*/
inline bool CreateThread(uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity, ThreadType *returnThread) {
	HANDLE handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, startRoutine, arg, CREATE_SUSPENDED, nullptr));

	if (handle == nullptr) {
		return false;
	}

	DWORD_PTR mask = 1ull << coreAffinity;
	SetThreadAffinityMask(handle, mask);
	
	returnThread->Handle = handle;
	returnThread->Id = GetThreadId(handle);
	ResumeThread(handle);

	return true;
}

/** Terminate the current thread */
inline void EndCurrentThread() {
	_endthreadex(0);
}

/**
* Join 'thread' with the current thread, blocking until 'thread' finishes
*
* @param thread    The thread to join
*/
inline void JoinThread(ThreadType thread) {
	WaitForSingleObject(thread.Handle, INFINITE);
}

/**
 * Get the number of hardware threads. This should take Hyperthreading, etc. into account
 *
 * @return    An number of hardware threads
 */
inline uint GetNumHardwareThreads() {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwNumberOfProcessors;
}

/**
 * Get the current thread
 *
 * @return    The current thread
 */
inline ThreadType GetCurrentThread() {
	Win32Thread result{
		::GetCurrentThread(),
		::GetCurrentThreadId()
	};

	return result;
}

/**
 * Set the core affinity for the current thread
 *
 * @param coreAffinity    The requested core affinity
 */
inline void SetCurrentThreadAffinity(size_t coreAffinity) {
	SetThreadAffinityMask(::GetCurrentThread(), coreAffinity);
}

/**
 * Create a native event
 *
 * @param event    The handle for the newly created event
 */
inline void CreateEvent(EventType *event) {
	event->event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	event->countWaiters = 0;
}

/**
 * Closes and event
 *
 * @param eventId    The event to close
 */
inline void CloseEvent(EventType eventId) {
	CloseHandle(eventId.event);
}

/**
* Wait for the given event to be signaled.
* The current thread will block until signaled or at least 'milliseconds' time has passed
*
* @param eventId         The event to wait on
* @param milliseconds    The maximum amount of time to wait for the event. Use EVENTWAIT_INFINITE to wait infinitely
*/
inline void WaitForEvent(EventType &eventId, uint32 milliseconds) {
	eventId.countWaiters.fetch_add(1u);
	DWORD retval = WaitForSingleObject(eventId.event, milliseconds);
	uint prev = eventId.countWaiters.fetch_sub(1u);
	if (1 == prev) {
		// we were the last to awaken, so reset event.
		ResetEvent(eventId.event);
	}
	assert(retval != WAIT_FAILED);
	assert(prev != 0);
}

/**
* Signal the given event. Any threads waiting on the event will resume.
*
* @param eventId    The even to signal
*/
inline void SignalEvent(EventType eventId) {
	SetEvent(eventId.event);
}

} // End of namespace FiberTaskingLib


#elif defined(FTL_POSIX_THREADS)

#include <pthread.h>
#include <unistd.h>


namespace FiberTaskingLib {

typedef pthread_t ThreadType;
struct EventType {
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
};
const uint32 EVENTWAIT_INFINITE = -1;

typedef void *(*ThreadStartRoutine)(void *arg);
#define FTL_THREAD_FUNC_RETURN_TYPE void *
#define FTL_THREAD_FUNC_DECL FTL_THREAD_FUNC_RETURN_TYPE
#define FTL_THREAD_FUNC_END return nullptr

/**
 * Create a native thread
 *
 * @param stackSize       The size of the stack
 * @param startRoutine    The start routine
 * @param arg             The argument for the start routine
 * @param returnThread    The handle for the newly created thread. Undefined if thread creation fails
 *
 * @return    True if thread creation succeeds, false if it fails
 */
inline bool CreateThread(uint stackSize, ThreadStartRoutine startRoutine, void *arg, ThreadType *returnThread) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	int success = pthread_create(returnThread, NULL, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

/**
 * Create a native thread
 *
 * @param stackSize       The size of the stack
 * @param startRoutine    The start routine
 * @param arg             The argument for the start routine
 * @param coreAffinity    The core affinity
 * @param returnThread    The handle for the newly created thread. Undefined if thread creation fails
 *
 * @return    True if thread creation succeeds, false if it fails
 */
inline bool CreateThread(uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity, ThreadType *returnThread) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	// TODO: OSX Thread Affinity
	#if !defined(FTL_OS_MAC) && !defined(FTL_OS_iOS)
		// Set core affinity
		cpu_set_t cpuSet;
		CPU_ZERO(&cpuSet);
		CPU_SET(coreAffinity, &cpuSet);
		pthread_attr_setaffinity_np(&threadAttr, sizeof(cpu_set_t), &cpuSet);
	#endif

	int success = pthread_create(returnThread, NULL, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

/** Terminate the current thread */
inline void EndCurrentThread() {
	pthread_exit(NULL);
}

/**
 * Join 'thread' with the current thread, blocking until 'thread' finishes
 *
 * @param thread    The thread to join
 */
inline void JoinThread(ThreadType thread) {
	pthread_join(thread, nullptr);
}

/**
* Get the number of hardware threads. This should take Hyperthreading, etc. into account
*
* @return    An number of hardware threads
*/
inline uint GetNumHardwareThreads() {
	return (uint)sysconf(_SC_NPROCESSORS_ONLN);
}

/**
* Get the current thread
*
* @return    The current thread
*/
inline ThreadType GetCurrentThread() {
	return pthread_self();
}

/**
* Set the core affinity for the current thread
*
* @param coreAffinity    The requested core affinity
*/
inline void SetCurrentThreadAffinity(size_t coreAffinity) {
	// TODO: OSX Thread Affinity
	#ifdef FTL_OS_LINUX
		cpu_set_t cpuSet;
		CPU_ZERO(&cpuSet);
		CPU_SET(coreAffinity, &cpuSet);

		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
	#endif
}

/**
* Create a native event
*
* @param event    The handle for the newly created event
*/
inline void CreateEvent(EventType *event) {
	*event = {PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
}

/**
 * Closes an event
 *
 * @param eventId    The event to close
 */
inline void CloseEvent(EventType eventId) {
	// No op
}

/**
 * Wait for the given event to be signaled.
 * The current thread will block until signaled or at least 'milliseconds' time has passed
 *
 * @param eventId         The event to wait on
 * @param milliseconds    The maximum amount of time to wait for the event. Use EVENTWAIT_INFINITE to wait infinitely
 */
inline void WaitForEvent(EventType &eventId, uint32 milliseconds) {
	pthread_mutex_lock(&eventId.mutex);

	if (milliseconds == EVENTWAIT_INFINITE) {
		pthread_cond_wait(&eventId.cond, &eventId.mutex);
	} else {
		timespec waittime;
		waittime.tv_sec = milliseconds / 1000;
		milliseconds -= waittime.tv_sec * 1000;
		waittime.tv_nsec = milliseconds * 1000;
		pthread_cond_timedwait(&eventId.cond, &eventId.mutex, &waittime);
	}

	pthread_mutex_unlock(&eventId.mutex);
}

/**
 * Signal the given event. Any threads waiting on the event will resume.
 *
 * @param eventId    The even to signal
 */
inline void SignalEvent(EventType eventId) {
	pthread_mutex_lock(&eventId.mutex);
	pthread_cond_broadcast(&eventId.cond);
	pthread_mutex_unlock(&eventId.mutex);
}

} // End of namespace FiberTaskingLib

#else
	#error No Thread library found
#endif
