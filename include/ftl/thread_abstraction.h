/**
 * FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015 - 2018
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

#pragma once

#include "ftl/config.h"
#include "ftl/typedefs.h"

#include <cassert>
#include <thread>

#if defined(FTL_WIN32_THREADS)

#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <Windows.h>

#	include <atomic>
#	include <process.h>

namespace ftl {

struct Win32Thread {
	HANDLE Handle;
	DWORD Id;
};

using ThreadType = Win32Thread;

struct EventType {
	HANDLE Event;
	std::atomic_ulong CountWaiters;
};

constexpr static uint32 kEventwaitInfinite = INFINITE;

using ThreadStartRoutine = uint(__stdcall *)(void *arg);
#	define FTL_THREAD_FUNC_RETURN_TYPE uint
#	define FTL_THREAD_FUNC_DECL FTL_THREAD_FUNC_RETURN_TYPE __stdcall
#	define FTL_THREAD_FUNC_END return 0

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
inline bool CreateThread(uint const stackSize, ThreadStartRoutine const startRoutine, void *const arg, ThreadType *const returnThread) {
	auto const handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, startRoutine, arg, 0U, nullptr));
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
inline bool CreateThread(uint const stackSize, ThreadStartRoutine const startRoutine, void *const arg, size_t const coreAffinity,
                         ThreadType *const returnThread) {
	auto const handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, startRoutine, arg, CREATE_SUSPENDED, nullptr));

	if (handle == nullptr) {
		return false;
	}

	DWORD_PTR const mask = 1ULL << coreAffinity;
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
inline void JoinThread(ThreadType const thread) {
	WaitForSingleObject(thread.Handle, INFINITE);
}

/**
 * Get the current thread
 *
 * @return    The current thread
 */
inline ThreadType GetCurrentThread() {
	Win32Thread const result{::GetCurrentThread(), GetCurrentThreadId()};

	return result;
}

/**
 * Set the core affinity for the current thread
 *
 * @param coreAffinity    The requested core affinity
 */
inline void SetCurrentThreadAffinity(size_t const coreAffinity) {
	SetThreadAffinityMask(::GetCurrentThread(), 1ULL << coreAffinity);
}

/**
 * Create a native event
 *
 * @param event    The handle for the newly created event
 */
inline void CreateEvent(EventType *const event) {
	event->Event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	event->CountWaiters = 0;
}

/**
 * Closes and event
 *
 * @param eventId    The event to close
 */
inline void CloseEvent(EventType const eventId) {
	CloseHandle(eventId.Event);
}

/**
 * Wait for the given event to be signaled.
 * The current thread will block until signaled or at least 'milliseconds' time has passed
 *
 * @param eventId         The event to wait on
 * @param milliseconds    The maximum amount of time to wait for the event. Use EVENTWAIT_INFINITE to wait infinitely
 */
#	pragma warning(push)
#	pragma warning(disable : 4189)
inline void WaitForEvent(EventType &eventId, uint32 const milliseconds) {
	eventId.CountWaiters.fetch_add(1U);
	DWORD const retval = WaitForSingleObject(eventId.Event, milliseconds);
	uint const prev = eventId.CountWaiters.fetch_sub(1U);
	if (1 == prev) {
		// we were the last to awaken, so reset event.
		ResetEvent(eventId.Event);
	}
	assert(retval != WAIT_FAILED);
	assert(prev != 0);
}
#	pragma warning(pop)

/**
 * Signal the given event. Any threads waiting on the event will resume.
 *
 * @param eventId    The even to signal
 */
inline void SignalEvent(EventType const eventId) {
	SetEvent(eventId.Event);
}

} // End of namespace ftl

#elif defined(FTL_POSIX_THREADS)

#	include <pthread.h>
#	include <unistd.h>

namespace ftl {

using ThreadType = pthread_t;
struct EventType {
	pthread_cond_t cond;
	pthread_mutex_t mutex;
};
constexpr static uint32 EVENTWAIT_INFINITE = std::numeric_limits<uint32>::max();

using ThreadStartRoutine = void *(*)(void *arg);
#	define FTL_THREAD_FUNC_RETURN_TYPE void *
#	define FTL_THREAD_FUNC_DECL FTL_THREAD_FUNC_RETURN_TYPE
#	define FTL_THREAD_FUNC_END return nullptr

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
inline bool CreateThread(uint const stackSize, ThreadStartRoutine const startRoutine, void *const arg, ThreadType *const returnThread) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	int success = pthread_create(returnThread, &threadAttr, startRoutine, arg);

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
inline bool CreateThread(uint const stackSize, ThreadStartRoutine const startRoutine, void *const arg, size_t const coreAffinity,
                         ThreadType *const returnThread) {
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

// TODO: OSX and MinGW Thread Affinity
#	if defined(FTL_OS_LINUX)
	// Set core affinity
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	CPU_SET(coreAffinity, &cpuSet);
	pthread_attr_setaffinity_np(&threadAttr, sizeof(cpu_set_t), &cpuSet);
#	else
	(void)coreAffinity;
#	endif

	int success = pthread_create(returnThread, &threadAttr, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

/** Terminate the current thread */
inline void EndCurrentThread() {
	pthread_exit(nullptr);
}

/**
 * Join 'thread' with the current thread, blocking until 'thread' finishes
 *
 * @param thread    The thread to join
 */
inline void JoinThread(ThreadType const thread) {
	pthread_join(thread, nullptr);
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
inline void SetCurrentThreadAffinity(size_t const coreAffinity) {
// TODO: OSX and MinGW Thread Affinity
#	if defined(FTL_OS_LINUX)
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	CPU_SET(coreAffinity, &cpuSet);

	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
#	else
	(void)coreAffinity;
#	endif
}

/**
 * Create a native event
 *
 * @param event    The handle for the newly created event
 */
inline void CreateEvent(EventType *const event) {
	*event = {PTHREAD_COND_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
}

/**
 * Closes an event
 *
 * @param eventId    The event to close
 */
inline void CloseEvent(EventType const /*eventId*/) {
	// No op
}

/**
 * Wait for the given event to be signaled.
 * The current thread will block until signaled or at least 'milliseconds' time has passed
 *
 * @param eventId         The event to wait on
 * @param milliseconds    The maximum amount of time to wait for the event. Use EVENTWAIT_INFINITE to wait infinitely
 */
inline void WaitForEvent(EventType &eventId, int64 milliseconds) {
	pthread_mutex_lock(&eventId.mutex);

	constexpr int64 mills_in_sec = 1000;

	if (milliseconds == EVENTWAIT_INFINITE) {
		pthread_cond_wait(&eventId.cond, &eventId.mutex);
	} else {
		timespec waittime{};
		waittime.tv_sec = milliseconds / mills_in_sec;
		milliseconds -= waittime.tv_sec * mills_in_sec;
		waittime.tv_nsec = milliseconds * mills_in_sec;
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

} // End of namespace ftl

#else
#	error No Thread library found
#endif

namespace ftl {

/**
 * Get the number of hardware threads. This should take Hyperthreading, etc. into account
 *
 * @return    An number of hardware threads
 */
inline uint GetNumHardwareThreads() {
	return std::thread::hardware_concurrency();
}

inline void YieldThread() {
	std::this_thread::yield();
}

} // End of namespace ftl
