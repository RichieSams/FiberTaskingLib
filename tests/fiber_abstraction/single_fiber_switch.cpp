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

struct SingleFiberArg {
	std::atomic_long Counter{0};
	ftl::Fiber MainFiber;
	ftl::Fiber OtherFiber;
};

void SingleFiberStart(void *arg) {
	auto *singleFiberArg = reinterpret_cast<SingleFiberArg *>(arg);

	singleFiberArg->Counter.fetch_add(1);

	singleFiberArg->OtherFiber.SwitchToFiber(&singleFiberArg->MainFiber);

	// We should never get here
	FAIL();
}

TEST_CASE("Single Fiber Switch", "[fiber]") {
	constexpr size_t kHalfMebibyte = 524288;

	SingleFiberArg singleFiberArg;
	singleFiberArg.Counter.store(0);
	singleFiberArg.OtherFiber = ftl::Fiber(kHalfMebibyte, SingleFiberStart, &singleFiberArg);

	singleFiberArg.MainFiber.SwitchToFiber(&singleFiberArg.OtherFiber);

	REQUIRE(singleFiberArg.Counter.load() == 1);
}
