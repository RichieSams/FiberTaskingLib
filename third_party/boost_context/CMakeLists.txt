##          Copyright Adrian Astley 2016 - 2018
## Distributed under the Boost Software License, Version 1.0.
##    (See accompanying file LICENSE_1_0.txt or copy at
##          http://www.boost.org/LICENSE_1_0.txt)

cmake_minimum_required(VERSION 3.2)
project(boost_context)

include(SetSourceGroup)


option(FTL_FIBER_CANARY_BYTES "Enable canary bytes in fiber switching logic, to help debug errors" OFF)


# Determine the target architecture
#
# Taken from Richard Maxwell's 'sewing' library: https://github.com/JodiTheTigger/sewing
# (MIT license)
#
# Enable the assemblers
if (WIN32 AND MSVC)
	enable_language(ASM_MASM)
else()
	enable_language(ASM)
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(CONTEXT_PLATFORM "x86_64")
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
	set(CONTEXT_PLATFORM "x86_64")
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "i386")
	set(CONTEXT_PLATFORM "i386")
endif()

if (CONTEXT_PLATFORM STREQUAL "x86_64")
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
		set(CONTEXT_PLATFORM "i386")
	endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(CONTEXT_PLATFORM "arm64")
	else()
		set(CONTEXT_PLATFORM "arm")
	endif()

	set(CONTEXT_OS  "aapcs")
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	set(CONTEXT_PLATFORM "arm64")

	set(CONTEXT_OS  "aapcs")
endif()

if (WIN32)
	# arm requires aapcs. So we have to check if arm already set the calling convention
	if (NOT CONTEXT_OS)
		set(CONTEXT_OS  "ms")
	endif()
	set(CONTEXT_ABI "pe")

	if (MSVC)
		set(CONTEXT_ASM "masm.asm")
	else()
		set(CONTEXT_ASM "gas.S")
	endif()
elseif(APPLE)
	set(CONTEXT_OS  "sysv")
	set(CONTEXT_ABI "macho")
	set(CONTEXT_ASM "gas.S")
elseif(UNIX)
	# arm requires aapcs. So we have to check if arm already set the calling convention
	if (NOT CONTEXT_OS)
		set(CONTEXT_OS  "sysv")
	endif()
	set(CONTEXT_ABI "elf")
	set(CONTEXT_ASM "gas.S")
endif()

if(FTL_FIBER_CANARY_BYTES)
	set(CONTEXT_CANARY "canary_")
else()
	set(CONTEXT_CANARY "")
endif()

message(STATUS "-- System processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "-- Target processor: ${CONTEXT_PLATFORM}")
message(STATUS "-- Target asm      : ${CONTEXT_CANARY}${CONTEXT_PLATFORM}_${CONTEXT_OS}_${CONTEXT_ABI}_${CONTEXT_ASM}")


SetSourceGroup(NAME Root
	PREFIX BOOST_CONTEXT
	SOURCE_FILES asm/make_${CONTEXT_CANARY}${CONTEXT_PLATFORM}_${CONTEXT_OS}_${CONTEXT_ABI}_${CONTEXT_ASM}
	             asm/jump_${CONTEXT_CANARY}${CONTEXT_PLATFORM}_${CONTEXT_OS}_${CONTEXT_ABI}_${CONTEXT_ASM}
	             include/boost_context/fcontext.h)

if (CONTEXT_ASM MATCHES "masm.asm" AND CONTEXT_PLATFORM MATCHES "i386")
	set_source_files_properties(asm/make_${CONTEXT_PLATFORM}_${CONTEXT_OS}_${CONTEXT_ABI}_${CONTEXT_ASM}
	                            asm/jump_${CONTEXT_PLATFORM}_${CONTEXT_OS}_${CONTEXT_ABI}_${CONTEXT_ASM}
	                            PROPERTIES
	                            COMPILE_FLAGS "/safeseh")
endif()

add_library(boost_context STATIC ${BOOST_CONTEXT_ROOT})
target_include_directories(boost_context INTERFACE include)
