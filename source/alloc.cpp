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

#include "ftl/alloc.h"

#include "ftl/assert.h"
#include "ftl/config.h"

#if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS)
#	include <sys/mman.h>
#	include <unistd.h>
#elif defined(FTL_OS_WINDOWS)
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <Windows.h>
#endif

namespace ftl {

#if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS)

void MemoryGuard(void *memory, size_t bytes) {
	int result = mprotect(memory, bytes, PROT_NONE);
	FTL_ASSERT("mprotect", !result);
#	if defined(NDEBUG)
	// Void out the result for release, so the compiler doesn't get cranky about an unused variable
	(void)result;
#	endif
}

void MemoryGuardRelease(void *memory, size_t bytes) {
	int const result = mprotect(memory, bytes, PROT_READ | PROT_WRITE);
	FTL_ASSERT("mprotect", !result);
#	if defined(NDEBUG)
	// Void out the result for release, so the compiler doesn't get cranky about an unused variable
	(void)result;
#	endif
}

size_t SystemPageSize() {
	return static_cast<size_t>(getpagesize());
}

void *AlignedAlloc(size_t size, size_t alignment) {
	void *returnPtr = nullptr;
	int const result = posix_memalign(&returnPtr, alignment, size);
	FTL_ASSERT("posix_memalign", !result);
#	if defined(NDEBUG)
	// Void out the result for release, so the compiler doesn't get cranky about an unused variable
	(void)result;
#	endif

	return returnPtr;
}

void AlignedFree(void *block) {
	free(block);
}

#elif defined(FTL_OS_WINDOWS)

void MemoryGuard(void *memory, size_t bytes) {
	DWORD ignored;

	BOOL const result = VirtualProtect(memory, bytes, PAGE_NOACCESS, &ignored);
	FTL_ASSERT("VirtualProtect", result);
	(void)result;
}

void MemoryGuardRelease(void *memory, size_t bytes) {
	DWORD ignored;

	BOOL const result = VirtualProtect(memory, bytes, PAGE_READWRITE, &ignored);
	FTL_ASSERT("VirtualProtect", result);
	(void)result;
}

size_t SystemPageSize() {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwPageSize;
}

void *AlignedAlloc(size_t size, size_t alignment) {
	return _aligned_malloc(size, alignment);
}

void AlignedFree(void *block) {
	_aligned_free(block);
}
#else
#	error "Unknown platform"
#endif

} // End of namespace ftl
