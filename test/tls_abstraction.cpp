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


TEST(TLSAbstraction, GetSet) {
	FiberTaskingLib::CreateTLSVariable(&tls_var, 1);
	FiberTaskingLib::SetThreadIndex(0u);


	FiberTaskingLib::SetTLSData(tls_var, 8u);
	GTEST_ASSERT_EQ(8u, FiberTaskingLib::GetTLSData(tls_var));

	FiberTaskingLib::SetTLSData(tls_var, 12345u);
	GTEST_ASSERT_EQ(12345u, FiberTaskingLib::GetTLSData(tls_var));

	FiberTaskingLib::SetTLSData(tls_var, 74234u);
	GTEST_ASSERT_EQ(74234u, FiberTaskingLib::GetTLSData(tls_var));


	FiberTaskingLib::DestroyTLSVariable(tls_var);
}


struct ThreadArg {
	uint threadIndex;
};


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


THREAD_FUNC_DECL ThreadStart(void *arg) {
	ThreadArg *threadArg = (ThreadArg *)arg;
	FiberTaskingLib::SetThreadIndex(threadArg->threadIndex);

	TestTLSGetSet();

	THREAD_FUNC_END;
}

TEST(TLSAbstraction, MultipleThreadGetSet) {
	FiberTaskingLib::CreateTLSVariable(&tls_var, 8);
	FiberTaskingLib::SetThreadIndex(0u);

	FiberTaskingLib::ThreadId threads[7];
	ThreadArg *args = new ThreadArg[7];
	for (uint i = 0; i < 7; ++i) {
		args[i].threadIndex = i + 1;
		FiberTaskingLib::FTLCreateThread(&threads[i], 524288u, ThreadStart, &args[i]);
	}

	TestTLSGetSet();

	FiberTaskingLib::FTLJoinThreads(7u, threads);
	FiberTaskingLib::DestroyTLSVariable(tls_var);
}
