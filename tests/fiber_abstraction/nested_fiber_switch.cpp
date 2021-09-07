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
	auto *multipleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	multipleFiberArg->Counter += 8;
	multipleFiberArg->FirstFiber.SwitchToFiber(&multipleFiberArg->SecondFiber);

	// Return from sixth
	// We just finished 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1
	// Let's do an intermediate check
	REQUIRE(((((((0ULL + 8ULL) * 3ULL) + 7ULL) * 6ULL) - 9ULL) * 2ULL) == multipleFiberArg->Counter);

	// Now run the rest of the sequence
	multipleFiberArg->Counter *= 4;
	multipleFiberArg->FirstFiber.SwitchToFiber(&multipleFiberArg->FifthFiber);

	// Return from fifth
	multipleFiberArg->Counter += 1;
	multipleFiberArg->FirstFiber.SwitchToFiber(&multipleFiberArg->ThirdFiber);

	// We should never get here
	FAIL();
}

void SecondLevelFiberStart(void *arg) {
	auto *multipleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	multipleFiberArg->Counter *= 3;
	multipleFiberArg->SecondFiber.SwitchToFiber(&multipleFiberArg->ThirdFiber);

	// Return from third
	multipleFiberArg->Counter += 9;
	multipleFiberArg->SecondFiber.SwitchToFiber(&multipleFiberArg->FourthFiber);

	// Return from fourth
	multipleFiberArg->Counter += 7;
	multipleFiberArg->SecondFiber.SwitchToFiber(&multipleFiberArg->FifthFiber);

	// We should never get here
	FAIL();
}

void ThirdLevelFiberStart(void *arg) {
	auto *multipleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	multipleFiberArg->Counter += 7;
	multipleFiberArg->ThirdFiber.SwitchToFiber(&multipleFiberArg->FourthFiber);

	// Return from first
	multipleFiberArg->Counter *= 3;
	multipleFiberArg->ThirdFiber.SwitchToFiber(&multipleFiberArg->SecondFiber);

	// Return from fifth
	multipleFiberArg->Counter *= 6;
	multipleFiberArg->ThirdFiber.SwitchToFiber(&multipleFiberArg->SixthFiber);

	// We should never get here
	FAIL();
}

void FourthLevelFiberStart(void *arg) {
	auto *multipleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	multipleFiberArg->Counter *= 6;
	multipleFiberArg->FourthFiber.SwitchToFiber(&multipleFiberArg->FifthFiber);

	// Return from second
	multipleFiberArg->Counter += 8;
	multipleFiberArg->FourthFiber.SwitchToFiber(&multipleFiberArg->SixthFiber);

	// Return from sixth
	multipleFiberArg->Counter *= 5;
	multipleFiberArg->FourthFiber.SwitchToFiber(&multipleFiberArg->SecondFiber);

	// We should never get here
	FAIL();
}

void FifthLevelFiberStart(void *arg) {
	auto *multipleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	multipleFiberArg->Counter -= 9;
	multipleFiberArg->FifthFiber.SwitchToFiber(&multipleFiberArg->SixthFiber);

	// Return from first
	multipleFiberArg->Counter *= 5;
	multipleFiberArg->FifthFiber.SwitchToFiber(&multipleFiberArg->FirstFiber);

	// Return from second
	multipleFiberArg->Counter += 1;
	multipleFiberArg->FifthFiber.SwitchToFiber(&multipleFiberArg->ThirdFiber);

	// We should never get here
	FAIL();
}

void SixthLevelFiberStart(void *arg) {
	auto *multipleFiberArg = reinterpret_cast<MultipleFiberArg *>(arg);

	multipleFiberArg->Counter *= 2;
	multipleFiberArg->SixthFiber.SwitchToFiber(&multipleFiberArg->FirstFiber);

	// Return from fourth
	multipleFiberArg->Counter -= 9;
	multipleFiberArg->SixthFiber.SwitchToFiber(&multipleFiberArg->FourthFiber);

	// Return from third
	multipleFiberArg->Counter -= 3;
	multipleFiberArg->SixthFiber.SwitchToFiber(&multipleFiberArg->MainFiber);

	// We should never get here
	FAIL();
}

TEST_CASE("Nested Fiber Switch", "[fiber]") {
	constexpr size_t kHalfMebibyte = 524288;

	MultipleFiberArg multipleFiberArg;
	multipleFiberArg.Counter = 0ULL;
	multipleFiberArg.MainFiber.InitFromCurrentContext(kHalfMebibyte);
	multipleFiberArg.FirstFiber = ftl::Fiber(kHalfMebibyte, FirstLevelFiberStart, &multipleFiberArg);
	multipleFiberArg.SecondFiber = ftl::Fiber(kHalfMebibyte, SecondLevelFiberStart, &multipleFiberArg);
	multipleFiberArg.ThirdFiber = ftl::Fiber(kHalfMebibyte, ThirdLevelFiberStart, &multipleFiberArg);
	multipleFiberArg.FourthFiber = ftl::Fiber(kHalfMebibyte, FourthLevelFiberStart, &multipleFiberArg);
	multipleFiberArg.FifthFiber = ftl::Fiber(kHalfMebibyte, FifthLevelFiberStart, &multipleFiberArg);
	multipleFiberArg.SixthFiber = ftl::Fiber(kHalfMebibyte, SixthLevelFiberStart, &multipleFiberArg);

	// The order should be:
	// 1 -> 2 -> 3 -> 4 -> 5 -> 6 -> 1 -> 5 -> 1 -> 3 -> 2 -> 4 -> 6 -> 4 -> 2 -> 5 -> 3 -> 6 -> Main

	multipleFiberArg.MainFiber.SwitchToFiber(&multipleFiberArg.FirstFiber);

	REQUIRE(((((((((((((((((((0ULL + 8ULL) * 3ULL) + 7ULL) * 6ULL) - 9ULL) * 2ULL) * 4) * 5) + 1) * 3) + 9) + 8) - 9) * 5) + 7) + 1) * 6) - 3) == multipleFiberArg.Counter);
}
