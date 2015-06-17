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

#include <gtest/include/gtest/gtest.h>


FiberTaskingLib::FiberId mainFiber;
FiberTaskingLib::FiberId otherFiber;

FIBER_START_FUNCTION(SingleFiberStart) {
    std::atomic_long *counter = (std::atomic_long *)arg;

    counter->fetch_add(1);

    FiberTaskingLib::FTLSwitchToFiber(otherFiber, mainFiber);

    // We should never get here
    FAIL();
    FiberTaskingLib::FTLSwitchToFiber(otherFiber, mainFiber);
}

TEST(FiberAbstraction, SingleFiberSwitch) {
    mainFiber = FiberTaskingLib::FTLConvertThreadToFiber();

    std::atomic_long *counter = new std::atomic_long();
    counter->store(0);

    otherFiber = FiberTaskingLib::FTLCreateFiber(524288, SingleFiberStart, (FiberTaskingLib::fiber_arg_t)counter);

    FiberTaskingLib::FTLSwitchToFiber(mainFiber, otherFiber);

    GTEST_ASSERT_EQ(1, counter->load());

    // Cleanup
    delete counter;
    FiberTaskingLib::FTLConvertFiberToThread();
    FiberTaskingLib::FTLDeleteFiber(mainFiber);
    FiberTaskingLib::FTLDeleteFiber(otherFiber);
}

