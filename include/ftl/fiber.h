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

#include "ftl/ftl_valgrind.h"

#include "boost_context/fcontext.h"

#include <stddef.h>

namespace ftl {

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
	Fiber(size_t stackSize, FiberStartRoutine startRoutine, void *arg);

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
	Fiber(Fiber &&other) noexcept
	        : Fiber() {
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
	~Fiber();

private:
	void *m_stack{nullptr};
	size_t m_systemPageSize{0};
	size_t m_stackSize{0};
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
	static void Swap(Fiber &first, Fiber &second) noexcept;
};

} // End of namespace ftl
