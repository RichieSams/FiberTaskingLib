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

#pragma once

#ifndef FTL_DEBUG
#	ifndef NDEBUG
#		define FTL_DEBUG 1
#	else
#		define FTL_DEBUG 0
#	endif
#endif

// Determine the OS
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#	define FTL_OS_WINDOWS
#elif defined(__APPLE__)
#	define FTL_OS_APPLE
#	include "TargetConditionals.h"

#	if defined(TARGET_OS_MAC)
#		define FTL_OS_MAC
#	elif defined(TARGET_OS_IPHONE)
#		define FTL_OS_iOS
#	else
#		error Unknown Apple platform
#	endif
#elif defined(__linux__)
#	define FTL_OS_LINUX
#endif

#if defined(_MSC_VER)
#	define FTL_WIN32_THREADS
#elif defined(__MINGW32__) || defined(__MINGW64__)
#	define FTL_POSIX_THREADS
#elif defined(FTL_OS_MAC) || defined(FTL_iOS) || defined(FTL_OS_LINUX)
#	include <unistd.h>

#	if defined(_POSIX_VERSION)
#		define FTL_POSIX_THREADS
#	endif
#endif

// ReSharper disable CppUnusedIncludeDirective
//               SSE on MSVC x86                            MSVC x64 has SSE              Clang/GCC define
#if (defined(_M_IX86_FP) && _M_IX86_FP >= 1) || (defined(_M_AMD64) || defined(_M_X64)) || defined(__SSE__)
#	include <emmintrin.h>
#	define FTL_PAUSE() _mm_pause()
#else
#	define FTL_PAUSE()
#endif

#if defined(FTL_POSIX_THREADS)
#	define FTL_NOINLINE_POSIX __attribute__((noinline))
#	define FTL_NOINLINE_WIN32
#	define FTL_NOINLINE FTL_NOINLINE_POSIX
#elif defined(FTL_WIN32_THREADS)
#	define FTL_NOINLINE_POSIX
#	define FTL_NOINLINE_WIN32 __declspec(noinline)
#	define FTL_NOINLINE FTL_NOINLINE_WIN32
#else
#	define FTL_NOINLINE_POSIX
#	define FTL_NOINLINE_WIN32
#	define FTL_NOINLINE
#endif

#ifdef __cpp_lib_hardware_interference_size
#	include <new>
#endif
// ReSharper restore CppUnusedIncludeDirective

namespace ftl {
#ifdef __cpp_lib_hardware_interference_size
constexpr static size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr static size_t kCacheLineSize = 64;
#endif
} // namespace ftl
