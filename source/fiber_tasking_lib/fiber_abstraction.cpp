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

#include "fiber_tasking_lib/fiber_abstraction.h"


namespace FiberTaskingLib {

#if defined(_WIN32)
	// Windows implementation
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>

	void *FTLConvertThreadToFiber() {
		return ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
	}
	bool FTLConvertFiberToThread() {
		return ConvertFiberToThread();
	}

	void *FTLCreateFiber(size_t stackSize, FiberStartRoutine startRoutine, void *arg) {
		return CreateFiberEx(stackSize, 0, FIBER_FLAG_FLOAT_SWITCH, startRoutine, arg);
	}
	void FTLDeleteFiber(void *fiber) {
		DeleteFiber(fiber);
	}

	void FTLSwitchToFiber(void *fiber) {
		SwitchToFiber(fiber);
	}

	void *FTLGetCurrentFiber() {
		return GetCurrentFiber();
	}

#elif defined(__linux__) 


	
#endif


} // End of namespace FiberTaskingLib
