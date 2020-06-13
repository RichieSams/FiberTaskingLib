###
 # FiberTaskingLib - A tasking library that uses fibers for efficient task switching
 #
 # This library was created as a proof of concept of the ideas presented by
 # Christian Gyrling in his 2015 GDC Talk 'Parallelizing the Naughty Dog Engine Using Fibers'
 #
 # http://gdcvault.com/play/1022186/Parallelizing-the-Naughty-Dog-Engine
 #
 # FiberTaskingLib is the legal property of Adrian Astley
 # Copyright Adrian Astley 2015 - 2018
 #
 # Licensed under the Apache License, Version 2.0 (the "License");
 # you may not use this file except in compliance with the License.
 # You may obtain a copy of the License at
 # 
 # http://www.apache.org/licenses/LICENSE-2.0
 # 
 # Unless required by applicable law or agreed to in writing, software
 # distributed under the License is distributed on an "AS IS" BASIS,
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and
 # limitations under the License.
 ##

cmake_minimum_required(VERSION 3.8)
project(FiberTaskingLib CXX)

# Options
option(FTL_BUILD_TESTS "Build FiberTaskingLib tests" ON)
option(FTL_BUILD_BENCHMARKS "Build FiberTaskingLib benchmarks" ON)
option(FTL_VALGRIND "Link and test with Valgrind" OFF)
option(FTL_FIBER_STACK_GUARD_PAGES "Add guard pages around the fiber stacks" OFF)
option(FTL_CPP_17 "Enable C++17 features" OFF)
option(FTL_WERROR "Promote compiler warnings to errors." OFF)
option(FTL_DISABLE_ITERATOR_DEBUG "In MSVC, sets _ITERATOR_DEBUG_LEVEL=0. NOP for all other compilers." OFF)

set(FTL_NUM_WAITING_FIBER_SLOTS 4 CACHE STRING "Number of slots within ftl::TaskCounter for fibers to wait")

# Include Valgrind
if (FTL_VALGRIND)
	add_definitions(-DFTL_VALGRIND=1)
endif()

# Add guard pages
if (FTL_FIBER_STACK_GUARD_PAGES)
	add_definitions(-DFTL_FIBER_STACK_GUARD_PAGES=1)
endif()

# MSVC Iterator Issues
if (MSVC AND FTL_DISABLE_ITERATOR_DEBUG)
	add_definitions(-D_ITERATOR_DEBUG_LEVEL=0)
endif()

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

# Build benchmarks
if (FTL_BUILD_BENCHMARKS)
	add_subdirectory(benchmarks)
endif()
