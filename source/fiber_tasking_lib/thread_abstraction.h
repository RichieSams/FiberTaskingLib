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
 * Copyright Adrian Astley 2015
 */

#pragma once

#include "fiber_tasking_lib/typedefs.h"

#include <cassert>


#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <process.h>


#define THREAD_LOCAL __declspec( thread )

namespace FiberTaskingLib {

typedef HANDLE ThreadId;
typedef uint (__stdcall *ThreadStartRoutine)(void *arg);
typedef uint THREAD_FUNC_RETURN_TYPE;
typedef THREAD_FUNC_RETURN_TYPE __stdcall THREAD_FUNC_DECL;
	
inline bool FTLCreateThread(ThreadId* returnId, uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity) {
	*returnId = (ThreadId)_beginthreadex(nullptr, stackSize, startRoutine, arg, CREATE_SUSPENDED, nullptr);

	if (*returnId == nullptr) {
		return false;
	}

	DWORD_PTR mask = 1ull << coreAffinity;
	SetThreadAffinityMask(*returnId, mask);
	ResumeThread(*returnId);
}

inline bool FTLTerminateThread(ThreadId threadId) {
	return CloseHandle(threadId) == 0;
}

inline void FTLJoinThreads(uint numThreads, ThreadId *threads) {
	WaitForMultipleObjects(numThreads, threads, true, INFINITE);
}

inline uint FTLGetNumHardwareThreads() {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwNumberOfProcessors;
}

inline ThreadId FTLGetCurrentThread() {
	return GetCurrentThread();
}

inline void FTLSetCurrentThreadAffinity(size_t coreAffinity) {
	SetThreadAffinityMask(GetCurrentThread(), coreAffinity);
}

} // End of namespace FiberTaskingLib


#else // posix

#include <pthread.h>
#include <unistd.h>
#define THREAD_LOCAL __thread

namespace FiberTaskingLib {

typedef pthread_t ThreadId;
typedef void *(*ThreadStartRoutine)(void *arg);
typedef void * THREAD_FUNC_RETURN_TYPE;
typedef THREAD_FUNC_RETURN_TYPE THREAD_FUNC_DECL;

inline bool FTLCreateThread(ThreadId* returnId, uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	// Set core affinity
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	CPU_SET(coreAffinity, &cpuSet);
	pthread_attr_setaffinity_np(&threadAttr, sizeof(cpu_set_t), &cpuSet);
	
	int success = pthread_create(returnId, NULL, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

inline bool FTLTerminateThread(ThreadId threadId) {
	return pthread_cancel(threadId) == 0;
}

inline void FTLJoinThreads(uint numThreads, ThreadId *threads) {
	for (uint i = 0; i < numThreads; ++i) {
		pthread_join(threads[i], nullptr);
	}
}

inline uint FTLGetNumHardwareThreads() {
	return (uint)sysconf(_SC_NPROCESSORS_ONLN);
}

inline ThreadId FTLGetCurrentThread() {
	return pthread_self();
}

inline void FTLSetCurrentThreadAffinity(size_t coreAffinity) {
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	CPU_SET(coreAffinity, &cpuSet);

	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
}

} // End of namespace FiberTaskingLib

#endif // posix

