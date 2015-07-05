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

#include <atomic>

#include <gtest/gtest.h>


struct SingleFiberArg {
    std::atomic_long Counter;
    FiberTaskingLib::FiberId MainFiber;
    FiberTaskingLib::FiberId FirstFiber;
	FiberTaskingLib::FiberId SecondFiber;
	FiberTaskingLib::FiberId ThirdFiber;
	FiberTaskingLib::FiberId FourthFiber;
	FiberTaskingLib::FiberId FifthFiber;
	FiberTaskingLib::FiberId SixthFiber;
};

FIBER_START_FUNCTION(FirstLevelFiberStart) {
    SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

    singleFiberArg->Counter.fetch_add(1);

    FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FirstFiber, singleFiberArg->MainFiber);

    // We should never get here
    FAIL();
    FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FirstFiber, singleFiberArg->MainFiber);
}

TEST(FiberAbstraction, NestedFiberSwitch) {
    SingleFiberArg *singleFiberArg = new SingleFiberArg();
    singleFiberArg->Counter.store(0);
    singleFiberArg->MainFiber = FiberTaskingLib::FTLConvertThreadToFiber();
    singleFiberArg->FirstFiber = FiberTaskingLib::FTLCreateFiber(524288, FirstLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);

    FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->MainFiber, singleFiberArg->FirstFiber);

    GTEST_ASSERT_EQ(1, singleFiberArg->Counter.load());

    // Cleanup
    FiberTaskingLib::FTLConvertFiberToThread(singleFiberArg->MainFiber);
    FiberTaskingLib::FTLDeleteFiber(singleFiberArg->FirstFiber);
    delete singleFiberArg;
}

