#pragma once

#include <iostream>

#define FTL_STR(s) #s
#ifndef NDEBUG
#	define FTL_ASSERT(msg, expr)                                                                                                          \
		if (!(expr)) {                                                                                                                     \
			std::cerr << "FTL Assertion Failure: " << (msg) << ". Expr: " FTL_STR(expr) "\n";                                              \
			std::cerr << "Source File: " __FILE__ ":" << __LINE__ << "\n";                                                                 \
			std::abort();                                                                                                                  \
		}
#else
#	define FTL_ASSERT(msg, expr)
#endif
