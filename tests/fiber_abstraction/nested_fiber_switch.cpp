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

#include "fiber_tasking_lib/typedefs.h"
#include "fiber_tasking_lib/fiber.h"

#include <atomic>

#include <gtest/gtest.h>


struct MultipleFiberArg {
	uint64 Counter;
	FiberTaskingLib::Fiber MainFiber;
	FiberTaskingLib::Fiber FirstFiber;
	FiberTaskingLib::Fiber SecondFiber;
	FiberTaskingLib::Fiber ThirdFiber;
	FiberTaskingLib::Fiber FourthFiber;
	FiberTaskingLib::Fiber FifthFiber;
	FiberTaskingLib::Fiber SixthFiber;
};

void FirstLevelFiberStart(void *arg) {
	MultipleFiberArg *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter += 8;
	singleFiberArg->FirstFiber.SwitchToFiber(&singleFiberArg->SecondFiber);

	// Return from sixth
	// We just finished 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1
	// Let's do an intermediate check
	GTEST_ASSERT_EQ(((((((0ull + 8ull) * 3ull) + 7ull) * 6ull) - 9ull) * 2ull), singleFiberArg->Counter);


	// Now run the rest of the sequence
	singleFiberArg->Counter *= 4;
	singleFiberArg->FirstFiber.SwitchToFiber(&singleFiberArg->FifthFiber);

	// Return from fifth
	singleFiberArg->Counter += 1;
	singleFiberArg->FirstFiber.SwitchToFiber(&singleFiberArg->ThirdFiber);


	// We should never get here
	FAIL();
}

void SecondLevelFiberStart(void *arg) {
	MultipleFiberArg *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter *= 3;
	singleFiberArg->SecondFiber.SwitchToFiber(&singleFiberArg->ThirdFiber);

	// Return from third
	singleFiberArg->Counter += 9;
	singleFiberArg->SecondFiber.SwitchToFiber(&singleFiberArg->FourthFiber);

	// Return from fourth
	singleFiberArg->Counter += 7;
	singleFiberArg->SecondFiber.SwitchToFiber(&singleFiberArg->FifthFiber);


	// We should never get here
	FAIL();
}

void ThirdLevelFiberStart(void *arg) {
	MultipleFiberArg *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter += 7;
	singleFiberArg->ThirdFiber.SwitchToFiber(&singleFiberArg->FourthFiber);

	// Return from first
	singleFiberArg->Counter *= 3;
	singleFiberArg->ThirdFiber.SwitchToFiber(&singleFiberArg->SecondFiber);

	// Return from fifth
	singleFiberArg->Counter *= 6;
	singleFiberArg->ThirdFiber.SwitchToFiber(&singleFiberArg->SixthFiber);


	// We should never get here
	FAIL();
}

void FourthLevelFiberStart(void *arg) {
	MultipleFiberArg *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter *= 6;
	singleFiberArg->FourthFiber.SwitchToFiber(&singleFiberArg->FifthFiber);

	// Return from second
	singleFiberArg->Counter += 8;
	singleFiberArg->FourthFiber.SwitchToFiber(&singleFiberArg->SixthFiber);

	// Return from sixth
	singleFiberArg->Counter *= 5;
	singleFiberArg->FourthFiber.SwitchToFiber(&singleFiberArg->SecondFiber);


	// We should never get here
	FAIL();
}

void FifthLevelFiberStart(void *arg) {
	MultipleFiberArg *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter -= 9;
	singleFiberArg->FifthFiber.SwitchToFiber(&singleFiberArg->SixthFiber);

	// Return from first
	singleFiberArg->Counter *= 5;
	singleFiberArg->FifthFiber.SwitchToFiber(&singleFiberArg->FirstFiber);

	// Return from second
	singleFiberArg->Counter += 1;
	singleFiberArg->FifthFiber.SwitchToFiber(&singleFiberArg->ThirdFiber);


	// We should never get here
	FAIL();
}

void SixthLevelFiberStart(void *arg) {
	MultipleFiberArg *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter *= 2;
	singleFiberArg->SixthFiber.SwitchToFiber(&singleFiberArg->FirstFiber);

	// Return from fourth
	singleFiberArg->Counter -= 9;
	singleFiberArg->SixthFiber.SwitchToFiber(&singleFiberArg->FourthFiber);

	// Return from third
	singleFiberArg->Counter -= 3;
	singleFiberArg->SixthFiber.SwitchToFiber(&singleFiberArg->MainFiber);


	// We should never get here
	FAIL();
}

TEST(FiberAbstraction, NestedFiberSwitch) {
	MultipleFiberArg singleFiberArg;
	singleFiberArg.Counter = 0ull;
	singleFiberArg.FirstFiber = std::move(FiberTaskingLib::Fiber(524288, FirstLevelFiberStart, &singleFiberArg));
	singleFiberArg.SecondFiber = std::move(FiberTaskingLib::Fiber(524288, SecondLevelFiberStart, &singleFiberArg));
	singleFiberArg.ThirdFiber = std::move(FiberTaskingLib::Fiber(524288, ThirdLevelFiberStart, &singleFiberArg));
	singleFiberArg.FourthFiber = std::move(FiberTaskingLib::Fiber(524288, FourthLevelFiberStart, &singleFiberArg));
	singleFiberArg.FifthFiber = std::move(FiberTaskingLib::Fiber(524288, FifthLevelFiberStart, &singleFiberArg));
	singleFiberArg.SixthFiber = std::move(FiberTaskingLib::Fiber(524288, SixthLevelFiberStart, &singleFiberArg));

	// The order should be:
	// 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1 -> 5 -> 1 -> 3 -> 2 -> 4 -> 6 -> 4 -> 2 -> 5 -> 3 -> 6 -> Main

	singleFiberArg.MainFiber.SwitchToFiber(&singleFiberArg.FirstFiber);

	GTEST_ASSERT_EQ(((((((((((((((((((0ull + 8ull) * 3ull) + 7ull) * 6ull) - 9ull) * 2ull) * 4) * 5) + 1) * 3) + 9) + 8) - 9) * 5) + 7) + 1) * 6) - 3), singleFiberArg.Counter);
}

