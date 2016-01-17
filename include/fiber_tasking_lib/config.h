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

#pragma once

#if defined(_MSC_VER)
	#define FIBER_IMPL_SUPPORTS_TLS
#endif

#if defined(_MSC_VER)
	#define WIN32_FIBER_IMPL
#else
	#define BOOST_CONTEXT_FIBER_IMPL
#endif


#if __APPLE__
    #include "TargetConditionals.h"

    #if defined(TARGET_OS_MAC)
        #define FTL_OS_MAC
    #endif

    #if defined(TARGET_OS_IPHONE)
        #define FTL_OS_iOS
    #endif

#endif //__APPLE__


// Determine what stdlib type and version we have

#if defined(_LIBCPP_VERSION)
    // libc++
    #define FTL_STD_LIB_LIBCPP
#elif defined(__GLIBCPP__) || defined(__GLIBCXX__)
    // GNU libstdc++ 3
    #define FTL_STD_LIB_LIBSTDCPP3
#elif (defined(_YVALS) && !defined(__IBMCPP__)) || defined(_CPPLIB_VER)
    // Dinkumware Library (this has to appear after any possible replacement libraries):
    #define FTL_STD_LIB_DINKUMWARE
#endif


#if defined(FTL_STD_LIB_LIBCPP)
    #if __cplusplus < 201103
        #define FTL_NO_CXX11_STD_ALIGN
    #endif
#endif


#if defined(FTL_STD_LIB_LIBSTDCPP3)
    #ifdef __clang__
        #if __has_include(<experimental/any>)
            #define LIBSTDCPP3_VERSION 50100
        #elif __has_include(<shared_mutex>)
            #define LIBSTDCPP3_VERSION 40900
        #elif __has_include(<ext/cmath>)
            #define LIBSTDCPP3_VERSION 40800
        #elif __has_include(<scoped_allocator>)
            #define LIBSTDCPP3_VERSION 40700
        #elif __has_include(<typeindex>)
            #define LIBSTDCPP3_VERSION 40600
        #elif __has_include(<future>)
            #define LIBSTDCPP3_VERSION 40500
        #elif  __has_include(<ratio>)
            #define LIBSTDCPP3_VERSION 40400
        #elif __has_include(<array>)
            #define LIBSTDCPP3_VERSION 40300
        #endif
    #else
        #define LIBSTDCPP3_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
    #endif
    
    #if (LIBSTDCPP3_VERSION < 50100)
        #define FTL_NO_CXX11_STD_ALIGN
    #endif
#endif
