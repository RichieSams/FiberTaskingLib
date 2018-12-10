#pragma once

#include <cstdio>
#include <iostream>

#define FTL_EXPAND(s) s
#define FTL_STR_IMPL(s) #s
#define FTL_STR(s) FTL_EXPAND(FTL_STR_IMPL(s))
#ifndef NDEBUG
#	define FTL_ASSERT(msg, expr)                                                                                                          \
		if (!(expr)) {                                                                                                                     \
			std::fputs("FTL Assertion Failure: " FTL_STR(msg) ". Expr: " FTL_STR(expr) "\n", stderr);                                      \
			std::fputs("Source File: " __FILE__ ":" FTL_STR(__LINE__) "\n", stderr);                                                       \
			std::abort();                                                                                                                  \
		}
#else
#	define FTL_ASSERT(msg, expr)
#endif
