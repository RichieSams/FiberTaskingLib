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
constexpr static double kEscapeRadius = 2.0;

constexpr static double kRealMin = -2.0;
constexpr static double kRealMax = 1.0;
constexpr static double kImagMin = -1.0;
constexpr static double kImagMax = 1.0;

constexpr static unsigned kImageWidth = 800;
constexpr static unsigned kImageHeight = 800;

constexpr static unsigned kMaxIterations = 200;

void MandlebrotIteration(double cReal, double cImag, double *zReal, double *zImag, bool *escaped) {
	double zRealSquared = (*zReal) * (*zReal);
	double zImagSquared = (*zImag) * (*zImag);

	*zImag = 2.0 * (*zReal) * (*zImag) + cImag;
	*zReal = zRealSquared - zImagSquared + cReal;
	*escaped = zRealSquared + zImagSquared > 4.0;
}

uint64_t Mandlebrot() {
	uint64_t escapeCount = 0;

	for (unsigned y = 0; y < kImageHeight; ++y) {
		for (unsigned x = 0; x < kImageWidth; ++x) {
			// Convert pixel coordinate to complex number
			double cReal = kRealMin + ((float)x / kImageWidth) * (kRealMax - kRealMin);
			double cImag = kImagMin + ((float)y / kImageHeight) * (kImagMax - kImagMin);

			double zReal = cReal;
			double zImag = cImag;

			// Start iterating
			unsigned n = 0;
			for (; n < kMaxIterations; ++n) {
				bool escaped;
				MandlebrotIteration(cReal, cImag, &zReal, &zImag, &escaped);

				if (escaped) {
					break;
				}
			}

			escapeCount += n;
		}
	}

	return escapeCount;
}

NONIUS_BENCHMARK("Mandlebrot", [](nonius::chronometer meter) {
	meter.measure([] {
		return Mandlebrot();
	});
})
