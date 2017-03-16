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

#include "fiber_tasking_lib/config.h"

#include <boost_context/fcontext.h>

#include <cassert>
#include <cstdlib>
#include <algorithm>

#if defined(FTL_VALGRIND)
	#include <valgrind/valgrind.h>
#endif

#if defined(FTL_FIBER_STACK_GUARD_PAGES)
	#if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS)
		#include <sys/mman.h>
		#include <unistd.h>
	#elif defined(FTL_OS_WINDOWS)
		#define WIN32_LEAN_AND_MEAN
		#include <Windows.h>
	#endif
#endif


namespace FiberTaskingLib {

#if defined(FTL_VALGRIND)
	#define FTL_VALGRIND_ID uint m_stackId

	#define FTL_VALGRIND_REGISTER(s, e) \
		m_stackId = VALGRIND_STACK_REGISTER(s, e)

	#define SEW_VALGRIND_DEREGISTER() VALGRIND_STACK_DEREGISTER(m_stackId)
#else
	#define FTL_VALGRIND_ID
	#define FTL_VALGRIND_REGISTER(s, e)
	#define FTL_VALGRIND_DEREGISTER()
#endif


inline void MemoryGuard(void *memory, size_t bytes);
inline void MemoryGuardRelease(void *memory, size_t bytes);
inline std::size_t SystemPageSize();
inline void *AlignedAlloc(std::size_t size, std::size_t alignment);
inline void AlignedFree(void *block);
inline std::size_t RoundUp(std::size_t numToRound, std::size_t multiple);

typedef void (*FiberStartRoutine)(void *arg);


class Fiber {
public:
	/**
	 * Default constructor
	 * Nothing is allocated. This can be used as a thread fiber.
	 */
	Fiber()
		: m_stack(nullptr),
		  m_systemPageSize(0),
		  m_stackSize(0),
		  m_context(nullptr),
		  m_arg(0) {
	}
	/**
	 * Allocates a stack and sets it up to start executing 'startRoutine' when first switched to
	 *
	 * @param stackSize        The stack size for the fiber. If guard pages are being used, this will be rounded up to the next multiple of the system page size
	 * @param startRoutine     The function to run when the fiber first starts
	 * @param arg              The argument to pass to 'startRoutine'
	 */
	Fiber(std::size_t stackSize, FiberStartRoutine startRoutine, void *arg)
			: m_arg(arg) {
		#if defined(FTL_FIBER_STACK_GUARD_PAGES)
			m_systemPageSize = SystemPageSize();
		#else
			m_systemPageSize = 0;
		#endif

		m_stackSize = RoundUp(stackSize, m_systemPageSize);
		// We add a guard page both the top and the bottom of the stack
		m_stack = AlignedAlloc(m_systemPageSize + m_stackSize + m_systemPageSize, m_systemPageSize);
		m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + m_systemPageSize + stackSize, stackSize, startRoutine);
		
		FTL_VALGRIND_REGISTER(static_cast<char *>(m_stack) + m_systemPageSize + stackSize, static_cast<char *>(m_stack) + m_systemPageSize);
		#if defined(FTL_FIBER_STACK_GUARD_PAGES)
			MemoryGuard(static_cast<char *>(m_stack), m_systemPageSize);
			MemoryGuard(static_cast<char *>(m_stack) + m_systemPageSize + stackSize, m_systemPageSize);
		#endif
	}

	/**
	 * Deleted copy constructor
	 * It makes no sense to copy a stack and its corresponding context. Therefore, we explicitly forbid it.
	 */
	Fiber(const Fiber &other) = delete;
	/**
	 * Move constructor
	 * This does a swap() of all the member variables
	 *
	 * @param other
	 *
	 * @return
	 */
	Fiber(Fiber &&other)
			: Fiber() {
		swap(*this, other);
	}

	/**
	 * Move assignment operator
	 * This does a swap() of all the member variables
	 *
	 * @param other    The fiber to move
	 */
	Fiber &operator=(Fiber &&other) {
		swap(*this, other);

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
	void *m_stack;
	std::size_t m_systemPageSize;
	std::size_t m_stackSize;
	boost_context::fcontext_t m_context;
	void *m_arg;
	FTL_VALGRIND_ID;

public:
	/**
	 * Saves the current stack context and then switches to the given fiber
	 * Execution will resume here once another fiber switches to this fiber
	 *
	 * @param fiber    The fiber to switch to
	 */
	void SwitchToFiber(Fiber *fiber) {
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
	void Reset(FiberStartRoutine startRoutine, void *arg) {
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
	void swap(Fiber &first, Fiber &second) {
		using std::swap;

		swap(first.m_stack, second.m_stack);
		swap(first.m_systemPageSize, second.m_systemPageSize);
		swap(first.m_stackSize, second.m_stackSize);
		swap(first.m_context, second.m_context);
		swap(first.m_arg, second.m_arg);
	}
};

#if defined(FTL_FIBER_STACK_GUARD_PAGES)
	#if defined(FTL_OS_LINUX) || defined(FTL_OS_MAC) || defined(FTL_iOS)
		inline void MemoryGuard(void *memory, size_t bytes) {
			int result = mprotect(memory, bytes, PROT_NONE);
			if(result) {
				perror("mprotect failed with error:");
				assert(!result);
			}
		}

		inline void MemoryGuardRelease(void *memory, size_t bytes) {
			int result = mprotect(memory, bytes, PROT_READ | PROT_WRITE);
			if(result) {
				perror("mprotect failed with error:");
				assert(!result);
			}
		}

		inline std::size_t SystemPageSize() {
			int pageSize = getpagesize();
			return pageSize;
		}

		inline void *AlignedAlloc(std::size_t size, std::size_t alignment) {
			void *returnPtr;
			posix_memalign(&returnPtr, alignment, size);

			return returnPtr;
		}

		inline void AlignedFree(void *block) {
			free(block);
		}
	#elif defined(FTL_OS_WINDOWS)
		inline void MemoryGuard(void *memory, size_t bytes) {
			DWORD ignored;

			BOOL result = VirtualProtect(memory, bytes, PAGE_NOACCESS, &ignored);
			assert(result);
		}

		inline void MemoryGuardRelease(void *memory, size_t bytes) {
			DWORD ignored;

			BOOL result = VirtualProtect(memory, bytes, PAGE_READWRITE, &ignored);
			assert(result);
		}

		inline std::size_t SystemPageSize() {
			SYSTEM_INFO sysInfo;
			GetSystemInfo(&sysInfo);
			return sysInfo.dwPageSize;
		}

		inline void *AlignedAlloc(std::size_t size, std::size_t alignment) {
			return _aligned_malloc(size, alignment);
		}

		inline void AlignedFree(void *block) {
			_aligned_free(block);
		}
	#else
		#error "Need a way to protect memory for this platform".
	#endif
#else
	inline void MemoryGuard(void *memory, size_t bytes) {
		(void)memory;
		(void)bytes;
	}

	inline void MemoryGuardRelease(void *memory, size_t bytes) {
		(void)memory;
		(void)bytes;
	}

	inline std::size_t SystemPageSize() {
		return 0;
	}

	inline void *AlignedAlloc(std::size_t size, std::size_t alignment) {
		return malloc(size);
	}

	inline void AlignedFree(void *block) {
		free(block);
	}
#endif

inline std::size_t RoundUp(std::size_t numToRound, std::size_t multiple) {
	if (multiple == 0) {
		return numToRound;
	}

	std::size_t remainder = numToRound % multiple;
	if (remainder == 0)
		return numToRound;

	return numToRound + multiple - remainder;
}

} // End of namespace FiberTaskingLib
