/* FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 *
 * This library was created as a proof of concept of the ideas presented by
 * Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 *
 * http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 *
 * FiberTaskingLib is the legal property of Adrian Astley
 * Copyright Adrian Astley 2015
 */

#pragma once

#include "fiber_tasking_lib/portability.h"
#include "fiber_tasking_lib/tls_abstraction.h"

#include <cstddef>


#if defined(_MSC_VER)

// VC++ implementation
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace FiberTaskingLib {

typedef void (STDCALL *FiberStartRoutine)(void *arg);
typedef void * fiber_arg_t;
#define FIBER_START_FUNCTION(functionName) void STDCALL functionName(void *arg)
#define FIBER_START_FUNCTION_CLASS_IMPL(className, functionName) void className::functionName(void *arg)

typedef void * FiberId;

inline FiberId FTLConvertThreadToFiber() {
	return ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
}
inline void FTLConvertFiberToThread(FiberId fiber) {
	ConvertFiberToThread();
}

inline FiberId FTLCreateFiber(size_t stackSize, FiberStartRoutine startRoutine, fiber_arg_t arg) {
	return CreateFiberEx(stackSize, 0, FIBER_FLAG_FLOAT_SWITCH, startRoutine, arg);
}
inline void FTLDeleteFiber(FiberId fiber) {
	DeleteFiber(fiber);
}

inline void FTLSwitchToFiber(FiberId currentFiber, FiberId destFiber) {
	SwitchToFiber(destFiber);
}

inline FiberId FTLGetCurrentFiber() {
	return GetCurrentFiber();
}

inline void FTLSetCurrentFiber(FiberId currentFiber) {
	// No op
}

} // End of namespace FiberTaskingLib

#else
// Boost.Context implementation
#define BOOST_CONTEXT

#include <boost/context/fcontext.hpp>
#include <boost/context/fixedsize_stack.hpp>

namespace FiberTaskingLib {

typedef void (*FiberStartRoutine)(intptr_t arg);
typedef intptr_t fiber_arg_t;
#define FIBER_START_FUNCTION(functionName) void functionName(intptr_t arg)
#define FIBER_START_FUNCTION_CLASS_IMPL(className, functionName) void className::functionName(intptr_t arg)

class Fiber {
public:
	Fiber()
		: m_stack(nullptr),
		  m_arg(0ull) {
	}
	Fiber(std::size_t stackSize, FiberStartRoutine startRoutine, fiber_arg_t arg)
			: m_stack(new boost::context::fixedsize_stack(stackSize)),
			  m_stackContext(m_stack->allocate()),
			  m_arg(arg) {
		m_fiberContext = boost::context::make_fcontext(m_stackContext.sp, m_stackContext.size, startRoutine);
	}
	~Fiber() {
		if (m_stack) {
			m_stack->deallocate(m_stackContext);
		}
		delete m_stack;
	}

private:
	boost::context::fcontext_t m_fiberContext;
	boost::context::fixedsize_stack *m_stack;
	boost::context::stack_context m_stackContext;
	fiber_arg_t m_arg;

public:
	inline void SwitchToFiber(Fiber *fiber) {
		boost::context::jump_fcontext(&m_fiberContext, fiber->m_fiberContext, fiber->m_arg, true);
	}
};

typedef Fiber * FiberId;


inline FiberId FTLConvertThreadToFiber() {
	return new Fiber();
}
inline void FTLConvertFiberToThread(FiberId fiber) {
	delete fiber;
}

inline FiberId FTLCreateFiber(size_t stackSize, FiberStartRoutine startRoutine, fiber_arg_t arg) {
	return new Fiber(stackSize, startRoutine, arg);
}
inline void FTLDeleteFiber(FiberId fiber) {
	delete fiber;
}

inline void FTLSwitchToFiber(FiberId currentFiber, FiberId destFiber) {
	currentFiber->SwitchToFiber(destFiber);
}

extern TLS_VARIABLE(FiberId, tls_currentFiber);

inline FiberId FTLGetCurrentFiber() {
	return GetTLSData(tls_currentFiber);
}

inline void FTLSetCurrentFiber(FiberId currentFiber) {
	return SetTLSData(tls_currentFiber, currentFiber);
}

} // End of namespace FiberTaskingLib

#endif

