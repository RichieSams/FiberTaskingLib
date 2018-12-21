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

// ReSharper disable CppUnusedIncludeDirective
#include "ftl/assert.h"
#include "ftl/config.h"
#include "ftl/ftl_valgrind.h"

#include "boost_context/fcontext.h"

#include <algorithm>
#include <cstdlib>
// ReSharper restore CppUnusedIncludeDirective

#if defined(FTL_FIBER_STACK_GUARD_PAGES)
#	if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS)
#		include <sys/mman.h>
#		include <unistd.h>
#	elif defined(FTL_OS_WINDOWS)
#		define WIN32_LEAN_AND_MEAN
#		define NOMINMAX
#		include <Windows.h>
#	endif
#endif

namespace ftl {

inline void MemoryGuard(void *memory, size_t bytes);
inline void MemoryGuardRelease(void *memory, size_t bytes);
inline std::size_t SystemPageSize();
inline void *AlignedAlloc(std::size_t size, std::size_t alignment);
inline void AlignedFree(void *block);
inline std::size_t RoundUp(std::size_t numToRound, std::size_t multiple);

using FiberStartRoutine = void (*)(void *arg);

class Fiber {
public:
	/**
	 * Default constructor
	 * Nothing is allocated. This can be used as a thread fiber.
	 */
	Fiber() = default;
	/**
	 * Allocates a stack and sets it up to start executing 'startRoutine' when first switched to
	 *
	 * @param stackSize        The stack size for the fiber. If guard pages are being used, this will be rounded up to
	 * the next multiple of the system page size
	 * @param startRoutine     The function to run when the fiber first starts
	 * @param arg              The argument to pass to 'startRoutine'
	 */
	Fiber(std::size_t const stackSize, FiberStartRoutine const startRoutine, void *const arg) : m_arg(arg) {
#if defined(FTL_FIBER_STACK_GUARD_PAGES)
		m_systemPageSize = SystemPageSize();
#else
		m_systemPageSize = 0;
#endif

		m_stackSize = RoundUp(stackSize, m_systemPageSize);
		// We add a guard page both the top and the bottom of the stack
		m_stack = AlignedAlloc(m_systemPageSize + m_stackSize + m_systemPageSize, m_systemPageSize);
		m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + m_systemPageSize + stackSize, stackSize, startRoutine);

		FTL_VALGRIND_REGISTER(static_cast<char *>(m_stack) + m_systemPageSize, static_cast<char *>(m_stack) + m_systemPageSize + stackSize);
#if defined(FTL_FIBER_STACK_GUARD_PAGES)
		MemoryGuard(static_cast<char *>(m_stack), m_systemPageSize);
		MemoryGuard(static_cast<char *>(m_stack) + m_systemPageSize + stackSize, m_systemPageSize);
#endif
	}

	/**
	 * Deleted copy constructor
	 * It makes no sense to copy a stack and its corresponding context. Therefore, we explicitly forbid it.
	 */
	Fiber(Fiber const &other) = delete;
	/**
	 * Deleted copy assignment operator
	 * It makes no sense to copy a stack and its corresponding context. Therefore, we explicitly forbid it.
	 */
	Fiber &operator=(Fiber const &other) = delete;

	/**
	 * Move constructor
	 * This does a swap() of all the member variables
	 *
	 * @param other
	 *
	 * @return
	 */
	Fiber(Fiber &&other) noexcept : Fiber() {
		Swap(*this, other);
	}

	/**
	 * Move assignment operator
	 * This does a swap() of all the member variables
	 *
	 * @param other    The fiber to move
	 */
	Fiber &operator=(Fiber &&other) noexcept {
		Swap(*this, other);

		return *this;
	}
	~Fiber() {
		if (m_stack != nullptr) {
			if (m_systemPageSize != 0) {
				MemoryGuardRelease(static_cast<char *>(m_stack), m_systemPageSize);
				MemoryGuardRelease(static_cast<char *>(m_stack) + m_systemPageSize + m_stackSize, m_systemPageSize);
			}
			FTL_VALGRIND_DEREGISTER();

			AlignedFree(m_stack);
		}
	}

private:
	void *m_stack{nullptr};
	std::size_t m_systemPageSize{0};
	std::size_t m_stackSize{0};
	boost_context::fcontext_t m_context{nullptr};
	void *m_arg{nullptr};
	FTL_VALGRIND_ID

public:
	/**
	 * Saves the current stack context and then switches to the given fiber
	 * Execution will resume here once another fiber switches to this fiber
	 *
	 * @param fiber    The fiber to switch to
	 */
	void SwitchToFiber(Fiber *const fiber) {
		boost_context::jump_fcontext(&m_context, fiber->m_context, fiber->m_arg);
	}
	/**
	 * Re-initializes the stack with a new startRoutine and arg
	 *
	 * NOTE: This can NOT be called on a fiber that has m_stack == nullptr || m_stackSize == 0
	 *       AKA, a default constructed fiber.
	 *
	 * @param startRoutine    The function to run when the fiber is next switched to
	 * @param arg             The arg for 'startRoutine'
	 *
	 * @return
	 */
	void Reset(FiberStartRoutine const startRoutine, void *const arg) {
		m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + m_stackSize, m_stackSize, startRoutine);
		m_arg = arg;
	}

private:
	/**
	 * Helper function for the move operators
	 * Swaps all the member variables
	 *
	 * @param first     The first fiber
	 * @param second    The second fiber
	 */
	static void Swap(Fiber &first, Fiber &second) noexcept {
		using std::swap;

		swap(first.m_stack, second.m_stack);
		swap(first.m_systemPageSize, second.m_systemPageSize);
		swap(first.m_stackSize, second.m_stackSize);
		swap(first.m_context, second.m_context);
		swap(first.m_arg, second.m_arg);
	}
};

#if defined(FTL_FIBER_STACK_GUARD_PAGES)
#	if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS)
inline void MemoryGuard(void *const memory, size_t bytes) {
	int result = mprotect(memory, bytes, PROT_NONE);
	FTL_ASSERT("mprotect", !result);
}

inline void MemoryGuardRelease(void *const memory, size_t const bytes) {
	int const result = mprotect(memory, bytes, PROT_READ | PROT_WRITE);
	FTL_ASSERT("mprotect", !result);
}

inline std::size_t SystemPageSize() {
	auto const pageSize = static_cast<std::size_t>(getpagesize());
	return pageSize;
}

inline void *AlignedAlloc(std::size_t const size, std::size_t const alignment) {
	void *returnPtr;
	posix_memalign(&returnPtr, alignment, size);

	return returnPtr;
}

inline void AlignedFree(void *const block) {
	free(block);
}
#	elif defined(FTL_OS_WINDOWS)
inline void MemoryGuard(void *const memory, size_t const bytes) {
	DWORD ignored;

	BOOL const result = VirtualProtect(memory, bytes, PAGE_NOACCESS, &ignored);
	FTL_ASSERT("VirtualProtect", result);
}

inline void MemoryGuardRelease(void *const memory, size_t const bytes) {
	DWORD ignored;

	BOOL const result = VirtualProtect(memory, bytes, PAGE_READWRITE, &ignored);
	FTL_ASSERT("VirtualProtect", result);
}

inline std::size_t SystemPageSize() {
	SYSTEM_INFO sysInfo;
	GetSystemInfo(&sysInfo);
	return sysInfo.dwPageSize;
}

inline void *AlignedAlloc(std::size_t const size, std::size_t const alignment) {
	return _aligned_malloc(size, alignment);
}

inline void AlignedFree(void *const block) {
	_aligned_free(block);
}
#	else
#		error "Need a way to protect memory for this platform".
#	endif
#else
inline void MemoryGuard(void *const memory, size_t const bytes) {
	(void)memory;
	(void)bytes;
}

inline void MemoryGuardRelease(void *const memory, size_t const bytes) {
	(void)memory;
	(void)bytes;
}

inline std::size_t SystemPageSize() {
	return 0;
}

inline void *AlignedAlloc(std::size_t const size, std::size_t const /*alignment*/) {
	return malloc(size);
}

inline void AlignedFree(void *const block) {
	free(block);
}
#endif

inline std::size_t RoundUp(std::size_t const numToRound, std::size_t const multiple) {
	if (multiple == 0) {
		return numToRound;
	}

	std::size_t const remainder = numToRound % multiple;
	if (remainder == 0) {
		return numToRound;
	}

	return numToRound + multiple - remainder;
}

} // End of namespace ftl
