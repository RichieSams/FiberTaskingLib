cmake_minimum_required(VERSION 2.8)
project(FiberTaskingLib)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(Config)
Config()

# Options
OPTION(FTL_BUILD_TESTS    "Build FiberTaskingLib tests" ON)

# CTest needs to be included as soon as possible
if (FTL_BUILD_TESTS)
	include(CTest)
endif()


# Add third party libs
# EASTL
include_directories(third_party/EASTL/include)

# GTest
if (FTL_BUILD_TESTS)
	include_directories(third_party/gtest/include)
endif()

add_subdirectory(third_party)


# Build FiberTaskingLib
include_directories(include)
add_subdirectory(source)

# Build Tests
if (FTL_BUILD_TESTS)
	add_subdirectory(tests)
endif()
