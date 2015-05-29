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


namespace FiberTaskingLib {
	// Declarations
	typedef void (__stdcall *FiberStartRoutine)(void *arg);

	void *FTLConvertThreadToFiber();
	bool FTLConvertFiberToThread();

	void *FTLCreateFiber(size_t stackSize, FiberStartRoutine startRoutine, void *arg);
	void FTLDeleteFiber(void *fiber);

	void FTLSwitchToFiber(void *fiber);

	void *FTLGetCurrentFiber();
} // End of namespace FiberTaskingLib