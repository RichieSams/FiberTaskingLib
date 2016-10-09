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

#include <boost_context/fcontext.h>

#include <cstdlib>
#include <algorithm>

//#include <valgrind/valgrind.h>


namespace FiberTaskingLib {

typedef void (*FiberStartRoutine)(std::intptr_t arg);
#define FTL_STACK_GUARD 1024

class Fiber {
public:
	/**
	 * Default constructor
	 * Nothing is allocated. This can be used as a thread fiber.
	 */
	Fiber()
		: m_stack(nullptr),
		  m_stackSize(0),
		  m_context(nullptr), 
		  m_arg(0) {
	}
	/**
	 * Allocates a stack and sets it up to start executing 'startRoutine' when first switched to
	 *
	 * @param stackSize       The stack size for the fiber
	 * @param startRoutine    The function to run when the fiber first starts
	 * @param arg             The argument to pass to 'startRoutine'
	 */
	Fiber(std::size_t stackSize, FiberStartRoutine startRoutine, std::intptr_t arg)
			: m_stackSize(stackSize),
			  m_arg(arg) {
		m_stack = malloc(stackSize);
		m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + stackSize, stackSize, startRoutine);
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
		free(m_stack);
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
		swap(first.m_stackSize, second.m_stackSize);
		swap(first.m_context, second.m_context);
		swap(first.m_arg, second.m_arg);
	}

private:
	void *m_stack;
	std::size_t m_stackSize;
	boost_context::fcontext_t m_context;
	std::intptr_t m_arg;

public:
	/**
	 * Saves the current stack context and then switches to the given fiber
	 * Execution will resume here once another fiber switches to this fiber
	 *
	 * @param fiber    The fiber to switch to
	 */
	inline void SwitchToFiber(Fiber *fiber) {
		boost_context::jump_fcontext(&m_context, fiber->m_context, fiber->m_arg, 1);
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
	inline void Reset(FiberStartRoutine startRoutine, std::intptr_t arg) {
		m_context = boost_context::make_fcontext(static_cast<char *>(m_stack) + m_stackSize, m_stackSize, startRoutine);
		m_arg = arg;
	}
};

} // End of namespace FiberTaskingLib
