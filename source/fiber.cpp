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

#include "ftl/fiber.h"

#include "ftl/assert.h"
#include "ftl/config.h"

#include <algorithm>

#if defined(FTL_FIBER_STACK_GUARD_PAGES)
#	if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_OS_iOS) || defined(FTL_OS_WASM)
#		include <sys/mman.h>
#		include <unistd.h>
#	elif defined(FTL_OS_WINDOWS)
#		define WIN32_LEAN_AND_MEAN
#		define NOMINMAX
#		include <Windows.h>
#	endif
#endif

namespace ftl {

void MemoryGuard(void *memory, size_t bytes);
void MemoryGuardRelease(void *memory, size_t bytes);
size_t SystemPageSize();
void *AlignedAlloc(size_t size, size_t alignment);
void AlignedFree(void *block);
size_t RoundUp(size_t numToRound, size_t multiple);

#if defined(FTL_BOOST_CONTEXT_FIBERS)

Fiber::Fiber(size_t stackSize, FiberStartRoutine startRoutine, void *arg)
        : m_arg(arg) {
#	if defined(FTL_FIBER_STACK_GUARD_PAGES)
	m_systemPageSize = SystemPageSize();
#	else
	m_systemPageSize = 0;
#	endif

	m_stackSize = RoundUp(stackSize, m_systemPageSize);
	// We add a guard page both the top and the bottom of the stack
	m_stack = AlignedAlloc(m_systemPageSize + m_stackSize + m_systemPageSize, m_systemPageSize);
	m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + m_systemPageSize + m_stackSize, m_stackSize, startRoutine);

	FTL_VALGRIND_REGISTER(static_cast<char *>(m_stack) + m_systemPageSize, static_cast<char *>(m_stack) + m_systemPageSize + m_stackSize);
#	if defined(FTL_FIBER_STACK_GUARD_PAGES)
	MemoryGuard(static_cast<char *>(m_stack), m_systemPageSize);
	MemoryGuard(static_cast<char *>(m_stack) + m_systemPageSize + m_stackSize, m_systemPageSize);
#	endif
}

Fiber::~Fiber() {
	if (m_stack != nullptr) {
		if (m_systemPageSize != 0) {
			MemoryGuardRelease(static_cast<char *>(m_stack), m_systemPageSize);
			MemoryGuardRelease(static_cast<char *>(m_stack) + m_systemPageSize + m_stackSize, m_systemPageSize);
		}
		FTL_VALGRIND_DEREGISTER();

		AlignedFree(m_stack);
	}
}

void Fiber::InitFromCurrentContext(size_t const stackSize) {
	(void)stackSize;
	// Boost context doesn't need to be initialized for thread stacks
}

void Fiber::SwitchToFiber(Fiber *const fiber) {
	boost_context::jump_fcontext(&m_context, fiber->m_context, fiber->m_arg);
}

void Fiber::Reset(FiberStartRoutine const startRoutine, void *const arg) {
	m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + m_stackSize, m_stackSize, startRoutine);
	m_arg = arg;
}

void Fiber::Swap(Fiber &first, Fiber &second) noexcept {
	std::swap(first.m_stack, second.m_stack);
	std::swap(first.m_systemPageSize, second.m_systemPageSize);
	std::swap(first.m_stackSize, second.m_stackSize);
	std::swap(first.m_context, second.m_context);
	std::swap(first.m_arg, second.m_arg);
}

#elif defined(FTL_EMSCRIPTEN_FIBERS)

Fiber::Fiber(size_t const stackSize, FiberStartRoutine const startRoutine, void *const arg) {
#	if defined(FTL_FIBER_STACK_GUARD_PAGES)
	m_systemPageSize = SystemPageSize();
#	else
	m_systemPageSize = 0;
#	endif

	m_stackSize = RoundUp(stackSize, m_systemPageSize);
	// We add a guard page both the top and the bottom of the stack
	m_cStack = AlignedAlloc(m_systemPageSize + m_stackSize + m_systemPageSize, m_systemPageSize);
	m_asyncifyStack = AlignedAlloc(m_systemPageSize + m_stackSize + m_systemPageSize, m_systemPageSize);
	emscripten_fiber_init(&m_context, startRoutine, arg, static_cast<char *>(m_cStack) + m_systemPageSize + m_stackSize, m_stackSize, static_cast<char *>(m_asyncifyStack) + m_systemPageSize + m_stackSize, m_stackSize);

#	if defined(FTL_FIBER_STACK_GUARD_PAGES)
	MemoryGuard(static_cast<char *>(m_cStack), m_systemPageSize);
	MemoryGuard(static_cast<char *>(m_cStack) + m_systemPageSize + m_stackSize, m_systemPageSize);
	MemoryGuard(static_cast<char *>(m_asyncifyStack), m_systemPageSize);
	MemoryGuard(static_cast<char *>(m_asyncifyStack) + m_systemPageSize + m_stackSize, m_systemPageSize);
#	endif
}

Fiber::~Fiber() {
	if (m_cStack != nullptr) {
		if (m_systemPageSize != 0) {
			MemoryGuardRelease(static_cast<char *>(m_cStack), m_systemPageSize);
			MemoryGuardRelease(static_cast<char *>(m_cStack) + m_systemPageSize, m_systemPageSize);
		}
		AlignedFree(m_cStack);
	}
	if (m_asyncifyStack != nullptr) {
		if (m_systemPageSize != 0) {
			MemoryGuardRelease(static_cast<char *>(m_asyncifyStack), m_systemPageSize);
			MemoryGuardRelease(static_cast<char *>(m_asyncifyStack) + m_systemPageSize, m_systemPageSize);
		}
		AlignedFree(m_asyncifyStack);
	}
}

void Fiber::InitFromCurrentContext(size_t const stackSize) {
#	if defined(FTL_FIBER_STACK_GUARD_PAGES)
	m_systemPageSize = SystemPageSize();
#	else
	m_systemPageSize = 0;
#	endif

	m_stackSize = RoundUp(stackSize, m_systemPageSize);
	// We add a guard page both the top and the bottom of the stack
	m_asyncifyStack = AlignedAlloc(m_systemPageSize + m_stackSize + m_systemPageSize, m_systemPageSize);

	emscripten_fiber_init_from_current_context(&m_context, static_cast<char *>(m_asyncifyStack) + m_systemPageSize, m_stackSize);

#	if defined(FTL_FIBER_STACK_GUARD_PAGES)
	MemoryGuard(static_cast<char *>(m_asyncifyStack), m_systemPageSize);
	MemoryGuard(static_cast<char *>(m_asyncifyStack) + m_systemPageSize + m_stackSize, m_systemPageSize);
#	endif
}

void Fiber::SwitchToFiber(Fiber *const fiber) {
	emscripten_fiber_swap(&m_context, &fiber->m_context);
}

void Fiber::Reset(FiberStartRoutine const startRoutine, void *const arg) {
	emscripten_fiber_init(&m_context, startRoutine, arg, static_cast<char *>(m_cStack) + m_systemPageSize, m_stackSize, static_cast<char *>(m_asyncifyStack) + m_systemPageSize, m_stackSize);
}

void Fiber::Swap(Fiber &first, Fiber &second) noexcept {
	std::swap(first.m_cStack, second.m_cStack);
	std::swap(first.m_asyncifyStack, second.m_asyncifyStack);
	std::swap(first.m_systemPageSize, second.m_systemPageSize);
	std::swap(first.m_stackSize, second.m_stackSize);
	std::swap(first.m_context, second.m_context);
}

#endif

#if defined(FTL_FIBER_STACK_GUARD_PAGES)
#	if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS) || defined(FTL_OS_WASM)
void MemoryGuard(void *memory, size_t bytes) {
	int result = mprotect(memory, bytes, PROT_NONE);
	FTL_ASSERT("mprotect", !result);
#		if defined(NDEBUG)
	// Void out the result for release, so the compiler doesn't get cranky about an unused variable
	(void)result;
#		endif
}

void MemoryGuardRelease(void *memory, size_t bytes) {
	int const result = mprotect(memory, bytes, PROT_READ | PROT_WRITE);
	FTL_ASSERT("mprotect", !result);
#		if defined(NDEBUG)
	// Void out the result for release, so the compiler doesn't get cranky about an unused variable
	(void)result;
#		endif
}

size_t SystemPageSize() {
	return static_cast<size_t>(getpagesize());
}

void *AlignedAlloc(size_t size, size_t alignment) {
	void *returnPtr = nullptr;
	int const result = posix_memalign(&returnPtr, alignment, size);
	FTL_ASSERT("posix_memalign", !result);
#		if defined(NDEBUG)
	// Void out the result for release, so the compiler doesn't get cranky about an unused variable
	(void)result;
#		endif

	return returnPtr;
}

void AlignedFree(void *block) {
	free(block);
}
#	elif defined(FTL_OS_WINDOWS)
void MemoryGuard(void *memory, size_t bytes) {
	DWORD ignored;

	BOOL const result = VirtualProtect(memory, bytes, PAGE_NOACCESS, &ignored);
	FTL_ASSERT("VirtualProtect", result);
}

void MemoryGuardRelease(void *memory, size_t bytes) {
	DWORD ignored;

	BOOL const result = VirtualProtect(memory, bytes, PAGE_READWRITE, &ignored);
	FTL_ASSERT("VirtualProtect", result);
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
#	else
#		error "Need a way to protect memory for this platform".
#	endif
#else
void MemoryGuard(void *memory, size_t bytes) {
	(void)memory;
	(void)bytes;
}

void MemoryGuardRelease(void *memory, size_t bytes) {
	(void)memory;
	(void)bytes;
}

size_t SystemPageSize() {
	return 0;
}

void *AlignedAlloc(size_t size, size_t /*alignment*/) {
	return malloc(size);
}

void AlignedFree(void *block) {
	free(block);
}
#endif

size_t RoundUp(size_t numToRound, size_t multiple) {
	if (multiple == 0) {
		return numToRound;
	}

	size_t const remainder = numToRound % multiple;
	if (remainder == 0) {
		return numToRound;
	}

	return numToRound + multiple - remainder;
}

} // End of namespace ftl
