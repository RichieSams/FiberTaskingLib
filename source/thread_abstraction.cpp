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

#include "ftl/thread_abstraction.h"

#include <cassert>
#include <thread>


#if defined(FTL_WIN32_THREADS)
#include <Windows.h>

#include <atomic>
#include <process.h>


namespace ftl {

bool CreateThread(uint stackSize, ThreadStartRoutine startRoutine, void *arg, ThreadType *returnThread) {
	HANDLE handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, startRoutine, arg, 0u, nullptr));
//	SetThreadDescription(handle, L"ftl worker");

	returnThread->Handle = handle;
	returnThread->Id = GetThreadId(handle);

	return handle != nullptr;
}

bool CreateThread(uint stackSize, ThreadStartRoutine startRoutine, void *arg, size_t coreAffinity, ThreadType *returnThread) {
	HANDLE handle = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, stackSize, startRoutine, arg, CREATE_SUSPENDED, nullptr));
//	SetThreadDescription(handle, L"ftl worker");
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

void EndCurrentThread() {
	_endthreadex(0);
}

void JoinThread(ThreadType thread) {
	WaitForSingleObject(thread.Handle, INFINITE);
}

ThreadType GetCurrentThread() {
	Win32Thread result{
		::GetCurrentThread(),
		::GetCurrentThreadId()
	};

	return result;
}

void SetCurrentThreadAffinity(size_t coreAffinity) {
	SetThreadAffinityMask(::GetCurrentThread(), coreAffinity);
}

void CreateEvent(EventType *event) {
	event->event = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
	event->countWaiters = 0;
}

void CloseEvent(EventType eventId) {
	CloseHandle(eventId.event);
}

#pragma warning( push )
#pragma warning( disable : 4189 )
void WaitForEvent(EventType &eventId, uint32 milliseconds) {
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
#pragma warning ( pop )

void SignalEvent(EventType eventId) {
	SetEvent(eventId.event);
}

} // End of namespace ftl

#endif


namespace ftl {
	
uint GetNumHardwareThreads() {
	return std::thread::hardware_concurrency();
}

void YieldThread() {
	std::this_thread::yield();
}
	
} // End of namespace ftl
