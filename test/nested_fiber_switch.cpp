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

#include "fiber_tasking_lib/typedefs.h"
#include "fiber_tasking_lib/fiber_abstraction.h"

#include <atomic>

#include <gtest/gtest.h>


struct SingleFiberArg {
	uint64 Counter;
	FiberTaskingLib::FiberType MainFiber;
	FiberTaskingLib::FiberType FirstFiber;
	FiberTaskingLib::FiberType SecondFiber;
	FiberTaskingLib::FiberType ThirdFiber;
	FiberTaskingLib::FiberType FourthFiber;
	FiberTaskingLib::FiberType FifthFiber;
	FiberTaskingLib::FiberType SixthFiber;
};

FIBER_START_FUNCTION(FirstLevelFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter += 8;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FirstFiber, singleFiberArg->SecondFiber);

	// Return from sixth
	// We just finished 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1
	// Let's do an intermediate check
	GTEST_ASSERT_EQ(((((((0ull + 8ull) * 3ull) + 7ull) * 6ull) - 9ull) * 2ull), singleFiberArg->Counter);


	// Now run the rest of the sequence
	singleFiberArg->Counter *= 4;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FirstFiber, singleFiberArg->FifthFiber);

	// Return from fifth
	singleFiberArg->Counter += 1;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FirstFiber, singleFiberArg->ThirdFiber);


	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FirstFiber, singleFiberArg->MainFiber);
}

FIBER_START_FUNCTION(SecondLevelFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter *= 3;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SecondFiber, singleFiberArg->ThirdFiber);

	// Return from third
	singleFiberArg->Counter += 9;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SecondFiber, singleFiberArg->FourthFiber);

	// Return from fourth
	singleFiberArg->Counter += 7;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SecondFiber, singleFiberArg->FifthFiber);


	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SecondFiber, singleFiberArg->MainFiber);
}

FIBER_START_FUNCTION(ThirdLevelFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter += 7;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->ThirdFiber, singleFiberArg->FourthFiber);

	// Return from first
	singleFiberArg->Counter *= 3;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->ThirdFiber, singleFiberArg->SecondFiber);

	// Return from fifth
	singleFiberArg->Counter *= 6;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->ThirdFiber, singleFiberArg->SixthFiber);


	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->ThirdFiber, singleFiberArg->MainFiber);
}

FIBER_START_FUNCTION(FourthLevelFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter *= 6;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FourthFiber, singleFiberArg->FifthFiber);

	// Return from second
	singleFiberArg->Counter += 8;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FourthFiber, singleFiberArg->SixthFiber);

	// Return from sixth
	singleFiberArg->Counter *= 5;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FourthFiber, singleFiberArg->SecondFiber);


	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FourthFiber, singleFiberArg->MainFiber);
}

FIBER_START_FUNCTION(FifthLevelFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter -= 9;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FifthFiber, singleFiberArg->SixthFiber);

	// Return from first
	singleFiberArg->Counter *= 5;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FifthFiber, singleFiberArg->FirstFiber);

	// Return from second
	singleFiberArg->Counter += 1;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FifthFiber, singleFiberArg->ThirdFiber);


	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->FifthFiber, singleFiberArg->MainFiber);
}

FIBER_START_FUNCTION(SixthLevelFiberStart) {
	SingleFiberArg *singleFiberArg = (SingleFiberArg *)arg;

	singleFiberArg->Counter *= 2;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SixthFiber, singleFiberArg->FirstFiber);

	// Return from fourth
	singleFiberArg->Counter -= 9;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SixthFiber, singleFiberArg->FourthFiber);

	// Return from third
	singleFiberArg->Counter -= 3;
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SixthFiber, singleFiberArg->MainFiber);


	// We should never get here
	FAIL();
	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->SixthFiber, singleFiberArg->MainFiber);
}

TEST(FiberAbstraction, NestedFiberSwitch) {
	SingleFiberArg *singleFiberArg = new SingleFiberArg();
	singleFiberArg->Counter = 0ull;
	singleFiberArg->MainFiber = FiberTaskingLib::FTLConvertThreadToFiber();
	singleFiberArg->FirstFiber = FiberTaskingLib::FTLCreateFiber(524288, FirstLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);
	singleFiberArg->SecondFiber = FiberTaskingLib::FTLCreateFiber(524288, SecondLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);
	singleFiberArg->ThirdFiber = FiberTaskingLib::FTLCreateFiber(524288, ThirdLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);
	singleFiberArg->FourthFiber = FiberTaskingLib::FTLCreateFiber(524288, FourthLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);
	singleFiberArg->FifthFiber = FiberTaskingLib::FTLCreateFiber(524288, FifthLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);
	singleFiberArg->SixthFiber = FiberTaskingLib::FTLCreateFiber(524288, SixthLevelFiberStart, (FiberTaskingLib::fiber_arg_t)singleFiberArg);

	// The order should be:
	// 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1 -> 5 -> 1 -> 3 -> 2 -> 4 -> 6 -> 4 -> 2 -> 5 -> 3 -> 6 -> Main

	FiberTaskingLib::FTLSwitchToFiber(singleFiberArg->MainFiber, singleFiberArg->FirstFiber);

	GTEST_ASSERT_EQ(((((((((((((((((((0ull + 8ull) * 3ull) + 7ull) * 6ull) - 9ull) * 2ull) * 4) * 5) + 1) * 3) + 9) + 8) - 9) * 5) + 7) + 1) * 6) - 3), singleFiberArg->Counter);

	// Cleanup
	FiberTaskingLib::FTLConvertFiberToThread(singleFiberArg->MainFiber);
	FiberTaskingLib::FTLDeleteFiber(singleFiberArg->FirstFiber);
	delete singleFiberArg;
}

