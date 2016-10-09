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

#include "fiber_tasking_lib/fiber.h"

#include <atomic>

#include <gtest/gtest.h>


struct SingleFiberArg {
	std::atomic_long Counter;
	FiberTaskingLib::Fiber MainFiber;
	FiberTaskingLib::Fiber OtherFiber;
};

void SingleFiberStart(std::intptr_t arg) {
	SingleFiberArg *singleFiberArg = reinterpret_cast<SingleFiberArg *>(arg);

	singleFiberArg->Counter.fetch_add(1);

	singleFiberArg->OtherFiber.SwitchToFiber(&singleFiberArg->MainFiber);

	// We should never get here
	FAIL();
}

TEST(FiberAbstraction, SingleFiberSwitch) {
	SingleFiberArg singleFiberArg;
	singleFiberArg.Counter.store(0);
	singleFiberArg.OtherFiber = std::move(FiberTaskingLib::Fiber(512000, SingleFiberStart, reinterpret_cast<std::intptr_t>(&singleFiberArg)));

	singleFiberArg.MainFiber.SwitchToFiber(&singleFiberArg.OtherFiber);

	GTEST_ASSERT_EQ(1, singleFiberArg.Counter.load());
}

