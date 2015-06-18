cmake_minimum_required(VERSION 2.8)
project(FiberTaskingLib)

if(WIN32)
	message(FATAL_ERROR "For Windows builds, use the supplied Visual Studio files")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(source libs libs/EASTL/include libs/gtest/include ${Boost_INCLUDE_DIR})


# Build FiberTaskingLib
add_subdirectory(libs/EASTL)
add_subdirectory(source/fiber_tasking_lib)

# Build ExampleProject
add_subdirectory(source/example_project)

#Build Tests
include(CTest)

add_subdirectory(libs/gtest)
add_subdirectory(test)
