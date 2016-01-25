/** Copyright (c) 2013 Doug Binks
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgement in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

/* Modified for use by FiberTaskingLib
 *
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
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


#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <process.h>
#include <atomic>


namespace FiberTaskingLib {

typedef HANDLE ThreadType;
typedef DWORD ThreadId;
struct EventType {
	HANDLE event;
	std::atomic_ulong countWaiters;
};
const uint32 EVENTWAIT_INFINITE = INFINITE;

typedef uint(__stdcall *ThreadStartRoutine)(void *arg);
#define THREAD_FUNC_RETURN_TYPE uint
#define THREAD_FUNC_DECL THREAD_FUNC_RETURN_TYPE __stdcall
#define THREAD_FUNC_END return 0;

inline bool FTLCreateThread(ThreadType *returnId, uint stackSize, ThreadStartRoutine startRoutine, void *arg) {
	*returnId = (ThreadType)_beginthreadex(nullptr, stackSize, startRoutine, arg, 0u, nullptr);

	return *returnId != nullptr;
}

inline bool FTLCreateThread(ThreadType *returnId, uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity) {
	*returnId = (ThreadType)_beginthreadex(nullptr, stackSize, startRoutine, arg, CREATE_SUSPENDED, nullptr);

	if (*returnId == nullptr) {
		return false;
	}

	DWORD_PTR mask = 1ull << coreAffinity;
	SetThreadAffinityMask(*returnId, mask);
	ResumeThread(*returnId);

	return true;
}

inline void FTLEndCurrentThread() {
	_endthreadex(0);
}

inline void FTLCleanupThread(ThreadType threadId) {
	// No op
	// _endthread will automatically close the handle for us
}

inline void FTLJoinThreads(uint numThreads, ThreadType *threads) {
	WaitForMultipleObjects(numThreads, threads, true, INFINITE);
}

inline uint FTLGetNumHardwareThreads() {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwNumberOfProcessors;
}

inline ThreadType FTLGetCurrentThread() {
	return GetCurrentThread();
}

inline ThreadId FTLGetCurrentThreadId() {
	return GetCurrentThreadId();
}

inline void FTLSetCurrentThreadAffinity(size_t coreAffinity) {
	SetThreadAffinityMask(GetCurrentThread(), coreAffinity);
}

inline void FTLCreateEvent(EventType *event) {
	event->event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	event->countWaiters = 0;
}

inline void FTLCloseEvent(EventType eventId) {
	CloseHandle(eventId.event);
}

inline void FTLWaitForEvent(EventType &eventId, uint32 milliseconds) {
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

inline void FTLSignalEvent(EventType eventId) {
	SetEvent(eventId.event);
}

} // End of namespace FiberTaskingLib


#else // posix

#include <pthread.h>
#include <unistd.h>
#if defined(__linux__)
	#include <unistd.h>
	#include <sys/syscall.h>
#endif


namespace FiberTaskingLib {

typedef pthread_t ThreadType;
#if defined(__linux__)
	typedef pid_t ThreadId;
#elif defined(FTL_OS_MAC)
	typedef mach_port_t ThreadId;
#endif
struct EventType {
	pthread_cond_t  cond;
	pthread_mutex_t mutex;
};
const uint32 EVENTWAIT_INFINITE = -1;

typedef void *(*ThreadStartRoutine)(void *arg);
#define THREAD_FUNC_RETURN_TYPE void *
#define THREAD_FUNC_DECL THREAD_FUNC_RETURN_TYPE
#define THREAD_FUNC_END pthread_exit(NULL);

inline bool FTLCreateThread(ThreadType *returnId, uint stackSize, ThreadStartRoutine startRoutine, void *arg) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	int success = pthread_create(returnId, NULL, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

inline bool FTLCreateThread(ThreadType *returnId, uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	//:todo: OSX Thread Affinity
	#ifndef FTL_OS_MAC
		// Set core affinity
		cpu_set_t cpuSet;
		CPU_ZERO(&cpuSet);
		CPU_SET(coreAffinity, &cpuSet);
		pthread_attr_setaffinity_np(&threadAttr, sizeof(cpu_set_t), &cpuSet);
	#endif

	int success = pthread_create(returnId, NULL, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

inline void FTLEndCurrentThread() {
	pthread_exit(NULL);
}

inline void FTLCleanupThread(ThreadType threadId) {
	pthread_cancel(threadId);
}

inline void FTLJoinThreads(uint numThreads, ThreadType *threads) {
	for (uint i = 0; i < numThreads; ++i) {
		pthread_join(threads[i], nullptr);
	}
}

inline uint FTLGetNumHardwareThreads() {
	return (uint)sysconf(_SC_NPROCESSORS_ONLN);
}

inline ThreadType FTLGetCurrentThread() {
	return pthread_self();
}

#if defined(__linux__)
	inline ThreadId FTLGetCurrentThreadId() {
		return syscall(SYS_gettid);
	}
#elif defined(FTL_OS_MAC)
	inline ThreadId FTLGetCurrentThreadId() {
		return pthread_mach_thread_np(pthread_self());
	}
#else
	#error Implement FTLGetCurrentThreadId for this platform
#endif


inline void FTLSetCurrentThreadAffinity(size_t coreAffinity) {

	//:todo: OSX Thread Affinity
	#ifndef	FTL_OS_MAC
		cpu_set_t cpuSet;
		CPU_ZERO(&cpuSet);
		CPU_SET(coreAffinity, &cpuSet);

		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
	#endif

}

inline void FTLCreateEvent(EventType *event) {
	*event = {PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
}

inline void FTLCloseEvent(EventType eventId) {
	// No op
}

inline void FTLWaitForEvent(EventType &eventId, uint32 milliseconds) {
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

inline void FTLSignalEvent(EventType eventId) {
	pthread_mutex_lock(&eventId.mutex);
	pthread_cond_broadcast(&eventId.cond);
	pthread_mutex_unlock(&eventId.mutex);
}

} // End of namespace FiberTaskingLib

#endif // posix

