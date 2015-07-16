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

#include "fiber_tasking_lib/tls_abstraction.h"

#include <gtest/gtest.h>


TLS_VARIABLE(uint, tls_var);


void TestTLSGetSet() {
	for (uint i = 0; i < 10000; ++i) {
		FiberTaskingLib::SetTLSData(tls_var, 8u);
		GTEST_ASSERT_EQ(8u, FiberTaskingLib::GetTLSData(tls_var));

		FiberTaskingLib::SetTLSData(tls_var, 12345u);
		GTEST_ASSERT_EQ(12345u, FiberTaskingLib::GetTLSData(tls_var));

		FiberTaskingLib::SetTLSData(tls_var, 74234u);
		GTEST_ASSERT_EQ(74234u, FiberTaskingLib::GetTLSData(tls_var));
	}
}

TEST(TLSAbstraction, GetSet) {
	TestTLSGetSet();
}


struct ThreadArg {
	uint threadIndex;
};

THREAD_FUNC_DECL ThreadStart(void *arg) {
	ThreadArg *threadArg = (ThreadArg *)arg;

	TestTLSGetSet();

	THREAD_FUNC_END;
}

TEST(TLSAbstraction, MultipleThreadGetSet) {
	const uint kNumThreads = 8;

	FiberTaskingLib::ThreadType threads[kNumThreads];
	ThreadArg *args = new ThreadArg[kNumThreads];
	for (uint i = 0; i < kNumThreads; ++i) {
		args[i].threadIndex = i;
		FiberTaskingLib::FTLCreateThread(&threads[i], 5524288u, ThreadStart, &args[i]);
	}

	FiberTaskingLib::FTLJoinThreads(kNumThreads, threads);
}
