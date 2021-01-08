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

#include "ftl/thread_abstraction.h"

#if defined(FTL_WIN32_THREADS)

#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <windows.h>

#	include <process.h>
#	include <stdlib.h>

#	pragma warning(push)
#	pragma warning(disable : 4996) // 'mbstowcs': _CRT_SECURE_NO_WARNINGS

namespace ftl {

static void SetThreadName(HANDLE handle, const char *threadName) {
	const int bufLen = 128;
	WCHAR bufWide[bufLen];

	mbstowcs(bufWide, threadName, bufLen);
	bufWide[bufLen - 1] = '\0';

	// We can't call directly call "SetThreadDescription()" (
	// https://msdn.microsoft.com/en-us/library/windows/desktop/mt774976(v=vs.85).aspx ) here, as that would result in a vague DLL load
	// failure at launch, on any PC running Windows older than 10.0.14393. Luckily, we can work around this crash, and provide all the
	// benefits of "SetThreadDescription()", by manually poking into the local "kernel32.dll".
	HMODULE hMod = ::GetModuleHandleW(L"kernel32.dll");
	if (hMod) {
		using SetThreadDescriptionPtr_t = HRESULT(WINAPI *)(_In_ HANDLE hThread, _In_ PCWSTR lpThreadDescription);

		SetThreadDescriptionPtr_t funcPtr = reinterpret_cast<SetThreadDescriptionPtr_t>(::GetProcAddress(hMod, "SetThreadDescription"));
		if (funcPtr != nullptr) {
			funcPtr(handle, bufWide);
		} else {
			// Failed to assign thread name. This requires Windows 10 Creators Update 2017, or newer, to have
			// thread names associated with debugging, profiling, and crash dumps
		}
	}
}
#	pragma warning(pop)

bool CreateThread(size_t stackSize, ThreadStartRoutine startRoutine, void *arg, const char *name, ThreadType *returnThread) {
	returnThread->Handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, (unsigned)stackSize, startRoutine, arg, 0u, nullptr));
	SetThreadName(returnThread->Handle, name);
	returnThread->Id = ::GetThreadId(returnThread->Handle);

	return returnThread != nullptr;
}

bool CreateThread(size_t stackSize, ThreadStartRoutine startRoutine, void *arg, const char *name, size_t coreAffinity, ThreadType *returnThread) {
	returnThread->Handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, (unsigned)stackSize, startRoutine, arg, CREATE_SUSPENDED, nullptr));
	SetThreadName(returnThread->Handle, name);
	returnThread->Id = ::GetThreadId(returnThread->Handle);

	if (returnThread->Handle == nullptr) {
		return false;
	}

	DWORD_PTR mask = 1ull << coreAffinity;
	::SetThreadAffinityMask(returnThread, mask);
	::ResumeThread(returnThread);

	return true;
}

void EndCurrentThread() {
	_endthreadex(0);
}

bool JoinThread(ThreadType thread) {
	DWORD ret = ::WaitForSingleObject(thread.Handle, INFINITE);
	if (ret == WAIT_OBJECT_0) {
		return true;
	}

	if (ret == WAIT_ABANDONED) {
		return false;
	}

	return false;
}

ThreadType GetCurrentThread() {
	return {::GetCurrentThread(), ::GetCurrentThreadId()};
}

bool SetCurrentThreadAffinity(size_t coreAffinity) {
	DWORD_PTR ret = ::SetThreadAffinityMask(::GetCurrentThread(), 1ULL << coreAffinity);
	return ret != 0;
}

void SleepThread(int msDuration) {
	::Sleep(msDuration);
}

void YieldThread() {
	::SwitchToThread();
}

} // End of namespace ftl

#elif defined(FTL_POSIX_THREADS)

#	if defined(FTL_OS_LINUX)
#		include <features.h>
#	endif
#	include <pthread.h>
#	include <string.h>
#	include <unistd.h>

namespace ftl {

bool CreateThread(size_t stackSize, ThreadStartRoutine startRoutine, void *arg, const char *name, ThreadType *returnThread) {
	(void)name;

	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	int success = pthread_create(returnThread, &threadAttr, startRoutine, arg);

	// Cleanup
	pthread_attr_destroy(&threadAttr);

	return success == 0;
}

bool CreateThread(size_t stackSize, ThreadStartRoutine startRoutine, void *arg, const char *name, size_t coreAffinity, ThreadType *returnThread) {
	(void)name;

	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);

	// Set stack size
	pthread_attr_setstacksize(&threadAttr, stackSize);

	// TODO: OSX, MinGW, and musl Thread Affinity
	//       musl can set the affinity after the thread has been started (using pthread_setaffinity_np() )
	//       To set the affinity at thread creation, glibc created the pthread_attr_setaffinity_np() extension
#	if defined(FTL_OS_LINUX) && defined(__GLIBC__)
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

void EndCurrentThread() {
	pthread_exit(nullptr);
}

bool JoinThread(ThreadType thread) {
	const int ret = pthread_join(thread, nullptr);
	return ret == 0;
}

ThreadType GetCurrentThread() {
	return pthread_self();
}

bool SetCurrentThreadAffinity(size_t coreAffinity) {
	// TODO: OSX and MinGW Thread Affinity
#	if defined(FTL_OS_LINUX)
	cpu_set_t cpuSet;
	CPU_ZERO(&cpuSet);
	CPU_SET(coreAffinity, &cpuSet);

	int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuSet);
	if (ret != 0) {
		// Clang-tidy can't figure out the #ifdefs
		// NOLINTNEXTLINE(readability-simplify-boolean-expr)
		return false;
	}
#	else
	(void)coreAffinity;
#	endif

	return true;
}

void SleepThread(int msDuration) {
	usleep(static_cast<unsigned>(msDuration) * 1000);
}

void YieldThread() {
#	if defined(FTL_OS_LINUX)
	pthread_yield();
#	endif
}

} // End of namespace ftl

#else
#	error No Thread library found
#endif

#include <thread>

namespace ftl {

unsigned GetNumHardwareThreads() {
	return std::thread::hardware_concurrency();
}

} // End of namespace ftl
