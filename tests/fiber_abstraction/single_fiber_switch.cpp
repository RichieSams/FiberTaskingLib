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

#include "fiber_tasking_lib/fiber_abstraction.h"

#include <atomic>

#include <gtest/include/gtest/gtest.h>


struct SingleFiberArg {
	std::atomic_long Counter;
	FiberTaskingLib::FiberType MainFiber;
	FiberTaskingLib::FiberType OtherFiber;
};

FIBER_START_FUNCTION(SingleFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter.fetch_add(1);

	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->OtherFiber, singleFiberArg->MainFiber);

	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->OtherFiber, singleFiberArg->MainFiber);
}

TEST(FiberAbstraction, SingleFiberSwitch) {
	SingleFiberArg *singleFiberArg = new SingleFiberArg();
	singleFiberArg->Counter.store(0);
	singleFiberArg->MainFiber = FiberTaskingLib::FTLConvertThreadToFiber();
	singleFiberArg->OtherFiber = FiberTaskingLib::FTLCreateFiber(524288, SingleFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);

	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->MainFiber, singleFiberArg->OtherFiber);

	GTEST_ASSERT_EQ(1, singleFiberArg->Counter.load());

	// Cleanup
	FiberTaskingLib::FTLConvertFiberToThread(singleFiberArg->MainFiber);
	FiberTaskingLib::FTLDeleteFiber(singleFiberArg->OtherFiber);
	delete singleFiberArg;
}

