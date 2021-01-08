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

#include <stddef.h>

// Forward declare the platform types so we don't have to include their heavy header files
#if defined(FTL_WIN32_THREADS)

typedef void *HANDLE;        // NOLINT(modernize-use-using)
typedef unsigned long DWORD; // NOLINT(modernize-use-using)

#elif defined(FTL_POSIX_THREADS)

#	if defined(FTL_OS_LINUX)
typedef unsigned long int pthread_t; // NOLINT(modernize-use-using)
#	elif defined(FTL_OS_APPLE)
struct _opaque_pthread_t;
typedef struct _opaque_pthread_t *__darwin_pthread_t; // NOLINT(modernize-use-using)
typedef __darwin_pthread_t pthread_t;                 // NOLINT(modernize-use-using)
#	endif
#endif

namespace ftl {

// Create wrapper types so all our function signatures are the same
#if defined(FTL_WIN32_THREADS)

struct Win32Thread {
	HANDLE Handle;
	DWORD Id;
};

typedef Win32Thread ThreadType; // NOLINT(modernize-use-using)

typedef unsigned int(__stdcall *ThreadStartRoutine)(void *arg); // NOLINT(modernize-use-using)
#	define FTL_THREAD_FUNC_RETURN_TYPE unsigned int
#	define FTL_THREAD_FUNC_DECL FTL_THREAD_FUNC_RETURN_TYPE __stdcall
#	define FTL_THREAD_FUNC_END return 0

#elif defined(FTL_POSIX_THREADS)

typedef pthread_t ThreadType; // NOLINT(modernize-use-using)

typedef void *(*ThreadStartRoutine)(void *arg); // NOLINT(modernize-use-using)
#	define FTL_THREAD_FUNC_RETURN_TYPE void *
#	define FTL_THREAD_FUNC_DECL FTL_THREAD_FUNC_RETURN_TYPE
#	define FTL_THREAD_FUNC_END return nullptr

#else
#	error No Thread library found
#endif

/**
 * Create a native thread
 *
 * @param stackSize       The size of the stack
 * @param startRoutine    The start routine
 * @param arg             The argument for the start routine
 * @param name            The name of the thread
 * @param returnThread    The handle for the newly created thread. Undefined if thread creation fails
 * @return                True if thread creation succeeds, false if it fails
 */
bool CreateThread(size_t stackSize, ThreadStartRoutine startRoutine, void *arg, const char *name, ThreadType *returnThread);
/**
 * Create a native thread
 *
 * @param stackSize       The size of the stack
 * @param startRoutine    The start routine
 * @param arg             The argument for the start routine
 * @param name            The name of the thread
 * @param coreAffinity    The core affinity
 * @param returnThread    The handle for the newly created thread. Undefined if thread creation fails
 * @return                True if thread creation succeeds, false if it fails
 */
bool CreateThread(size_t stackSize, ThreadStartRoutine startRoutine, void *arg, const char *name, size_t coreAffinity, ThreadType *returnThread);

/**
 * Get the current thread
 *
 * @return    The current thread
 */
ThreadType GetCurrentThread();

/**
 * Terminate the current thread
 */
void EndCurrentThread();

/**
 * Join 'thread' with the current thread, blocking until 'thread' finishes
 *
 * @param thread    The thread to join
 */
bool JoinThread(ThreadType thread);

/**
 * Set the core affinity for the current thread
 *
 * @param coreAffinity    The requested core affinity
 */
bool SetCurrentThreadAffinity(size_t coreAffinity);

/**
 * Sleep the current thread
 *
 * @param msDuration    The number of milliseconds to sleep
 */
void SleepThread(int msDuration);

/**
 * Yield the rest of this thread's timeslice back to the scheduler
 */
void YieldThread();

/**
 * Get the number of hardware threads. This should take Hyperthreading, etc. into account
 *
 * @return    The number of hardware threads
 */
unsigned GetNumHardwareThreads();

} // namespace ftl
