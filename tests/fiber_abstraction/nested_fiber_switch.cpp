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

#include "ftl/fiber.h"

#include "catch2/catch.hpp"

#include <atomic>
#include <stdint.h>

struct MultipleFiberArg {
	uint64_t Counter{0};
	ftl::Fiber MainFiber;
	ftl::Fiber FirstFiber;
	ftl::Fiber SecondFiber;
	ftl::Fiber ThirdFiber;
	ftl::Fiber FourthFiber;
	ftl::Fiber FifthFiber;
	ftl::Fiber SixthFiber;
};

void FirstLevelFiberStart(void *arg) {
	auto *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	singleFiberArg->Counter += 8;
	singleFiberArg->FirstFiber.SwitchToFiber(&singleFiberArg->SecondFiber);

	// Return from sixth
	// We just finished 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1
	// Let's do an intermediate check
	REQUIRE(((((((0ULL + 8ULL) * 3ULL) + 7ULL) * 6ULL) - 9ULL) * 2ULL) == singleFiberArg->Counter);

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
	auto *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

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
	auto *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

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
	auto *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

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
	auto *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

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
	auto *singleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

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

TEST_CASE("Nested Fiber Switch", "[fiber]") {
	constexpr size_t kHalfMebibyte = 524288;

	MultipleFiberArg singleFiberArg;
	singleFiberArg.Counter = 0ULL;
	singleFiberArg.FirstFiber = ftl::Fiber(kHalfMebibyte, FirstLevelFiberStart, &singleFiberArg);
	singleFiberArg.SecondFiber = ftl::Fiber(kHalfMebibyte, SecondLevelFiberStart, &singleFiberArg);
	singleFiberArg.ThirdFiber = ftl::Fiber(kHalfMebibyte, ThirdLevelFiberStart, &singleFiberArg);
	singleFiberArg.FourthFiber = ftl::Fiber(kHalfMebibyte, FourthLevelFiberStart, &singleFiberArg);
	singleFiberArg.FifthFiber = ftl::Fiber(kHalfMebibyte, FifthLevelFiberStart, &singleFiberArg);
	singleFiberArg.SixthFiber = ftl::Fiber(kHalfMebibyte, SixthLevelFiberStart, &singleFiberArg);

	// The order should be:
	// 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1 -> 5 -> 1 -> 3 -> 2 -> 4 -> 6 -> 4 -> 2 -> 5 -> 3 -> 6 -> Main

	singleFiberArg.MainFiber.SwitchToFiber(&singleFiberArg.FirstFiber);

	REQUIRE(((((((((((((((((((0ULL + 8ULL) * 3ULL) + 7ULL) * 6ULL) - 9ULL) * 2ULL) * 4) * 5) + 1) * 3) + 9) + 8) - 9) * 5) + 7) + 1) * 6) - 3) == singleFiberArg.Counter);
}
