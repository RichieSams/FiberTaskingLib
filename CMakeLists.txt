cmake_minimum_required(VERSION 2.8)
project(FiberTaskingLib)

if(WIN32)
	message(FATAL_ERROR "For Windows builds, use the supplied Visual Studio files")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "build")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(Boost 1.56 COMPONENTS context REQUIRED)
find_package(Threads)

include_directories(source libs libs/EASTL/include ${Boost_INCLUDE_DIR} libs/gtest/include)

set(FIBER_TASKING_LIB_SRC
	libs/EASTL/src/allocator.cpp
	libs/EASTL/src/assert.cpp
	libs/EASTL/src/fixed_pool.cpp
	libs/EASTL/src/hashtable.cpp
	libs/EASTL/src/red_black_tree.cpp
	libs/EASTL/src/string.cpp
	source/fiber_tasking_lib/basic_linear_allocator.cpp
	source/fiber_tasking_lib/read_write_lock.cpp
	source/fiber_tasking_lib/tagged_heap.cpp
	source/fiber_tasking_lib/tagged_heap_backed_linear_allocator.cpp
	source/fiber_tasking_lib/task_scheduler.cpp)

add_library(FiberTaskingLib ${FIBER_TASKING_LIB_SRC})
target_link_libraries(FiberTaskingLib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})


set(EXAMPLE_PROJECT_SRC
	source/example_project/eastl_defaults.cpp
	source/example_project/main.cpp)

add_executable(ExampleProject ${EXAMPLE_PROJECT_SRC})
target_link_libraries(ExampleProject FiberTaskingLib)


enable_testing()
add_subdirectory(libs/gtest)

set (TEST_PROJECT_SRC
	test/calc_triangle_num.cpp
	test/eastl_defaults.cpp
	test/fiber_abstraction.cpp
	test/main.cpp
	test/maze.cpp
	test/producer_consumer.cpp
)

add_executable(FiberTaskingLib-test ${TEST_PROJECT_SRC})
target_link_libraries(FiberTaskingLib-test gtest gtest_main FiberTaskingLib)
