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

#include "nonius/nonius.hpp"

// Constants
volatile static uint64_t kFibinacciNumber = 1000;

uint64_t Fibonacci(uint64_t n) {
	if (n <= 1) {
		return n;
	}

	// Work our way up to n, accumulating as we go
	uint64_t prevFib = 0;
	uint64_t currFib = 1;
	for (uint64_t i = 0; i < n - 1; ++i) {
		uint64_t newFib = prevFib + currFib;
		prevFib = currFib;
		currFib = newFib;
	}

	return currFib;
}

NONIUS_BENCHMARK("Fibonacci", [](nonius::chronometer meter) {
	meter.measure([] {
		uint64_t total = 0;
		for (int i = 0; i < 4000; ++i) {
			total += Fibonacci(kFibinacciNumber + i);
		}

		return total;
	});
})
