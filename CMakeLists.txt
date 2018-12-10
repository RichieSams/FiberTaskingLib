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

cmake_minimum_required(VERSION 3.2)
project(FiberTaskingLib)

# Options
option(FTL_BUILD_TESTS "Build FiberTaskingLib tests" ON)
option(FTL_BUILD_BENCHMARKS "Build FiberTaskingLib benchmarks" ON)
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

# Warning flags
include(${CMAKE_SOURCE_DIR}/cmake/CheckAndAddFlag.cmake)

if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
	Check_And_Add_Flag(-fdiagnostics-color=always)
	Check_And_Add_Flag(-Wall)
	Check_And_Add_Flag(-Wextra)
	Check_And_Add_Flag(-Wpedantic)
	Check_And_Add_Flag(-Wconversion)
	Check_And_Add_Flag(-Wsign-conversion)
	Check_And_Add_Flag(-Wcast-align)
	Check_And_Add_Flag(-Wcast-qual)
	Check_And_Add_Flag(-Wctor-dtor-privacy)
	Check_And_Add_Flag(-Wdisabled-optimization)
	Check_And_Add_Flag(-Wdouble-promotion)
	Check_And_Add_Flag(-Wduplicated-branches)
	Check_And_Add_Flag(-Wduplicated-cond)
	Check_And_Add_Flag(-Wformat=2)
	Check_And_Add_Flag(-Wlogical-op)
	Check_And_Add_Flag(-Wmissing-include-dirs)
	Check_And_Add_Flag(-Wnoexcept)
	Check_And_Add_Flag(-Wnull-dereference)
	Check_And_Add_Flag(-Wold-style-cast)
	Check_And_Add_Flag(-Woverloaded-virtual)
	Check_And_Add_Flag(-Wshadow)
	Check_And_Add_Flag(-Wstrict-aliasing=1)
	Check_And_Add_Flag(-Wstrict-null-sentinel)
	Check_And_Add_Flag(-Wstrict-overflow=2)
	Check_And_Add_Flag(-Wswitch-default)
	Check_And_Add_Flag(-Wundef)
	Check_And_Add_Flag(-Wuseless-cast)
	Check_And_Add_Flag(-Wno-unknown-pragmas)
	Check_And_Add_Flag(-Wno-aligned-new)
else()
	Check_And_Add_Flag(/W4)
	Check_And_Add_Flag(/wd4324)
endif()

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

# Linter scripts
set(CMAKE_EXPORT_COMPILE_COMMANDS True CACHE BOOL "Export Compile Commands for Linting" FORCE)
configure_file(tools/lint-clang.bash.in tools/lint-clang.bash @ONLY)
configure_file(tools/format.bash.in tools/format.bash @ONLY)
