## FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 #
 # This library was created as a proof of concept of the ideas presented by
 # Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 #
 # http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 #
 # FiberTaskingLib is the legal property of Adrian Astley
 # Copyright Adrian Astley 2015 - 2016
 ##

cmake_minimum_required(VERSION 3.2)
project(FiberTaskingLib)

# Options
option(FTL_BUILD_TESTS "Build FiberTaskingLib tests" ON)
option(FTL_VALGRIND "Link and test with Valgrind" OFF)
option(FTL_FIBER_STACK_GUARD_PAGES "Add guard pages around the fiber stacks" OFF)

# Include Valgrind
if (FTL_VALGRIND)
	add_definitions(-DFTL_VALGRIND=1)
endif()

# Define the guard page size
if (FTL_FIBER_STACK_GUARD_PAGES)
	add_definitions(-DFTL_FIBER_STACK_GUARD_PAGES=1)
endif()

# CTest needs to be included as soon as possible
if (FTL_BUILD_TESTS)
	include(CTest)
endif()

set(CMAKE_CXX_STANDARD 11) # C++11...
set(CMAKE_CXX_STANDARD_REQUIRED ON) #...is required...
set(CMAKE_CXX_EXTENSIONS OFF) #...without compiler extensions like gnu++11

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Add third party libs
add_subdirectory(third_party)

# Build FiberTaskingLib
add_subdirectory(source)


# Build Tests
if (FTL_BUILD_TESTS)
	add_subdirectory(tests)
endif()
