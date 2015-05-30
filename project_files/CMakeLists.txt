cmake_minimum_required(VERSION 3.2)
project(FiberTaskingLib)

if(WIN32)
	message(FATAL_ERROR "For Windows builds, use the supplied Visual Studio files")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(../source ../libs ../libs/EASTL/include)

set(SOURCE_FILES
	../libs/EASTL/src/allocator.cpp
	../libs/EASTL/src/assert.cpp
	../libs/EASTL/src/fixed_pool.cpp
	../libs/EASTL/src/hashtable.cpp
	../libs/EASTL/src/red_black_tree.cpp
	../libs/EASTL/src/string.cpp
	../source/fiber_tasking_lib/basic_linear_allocator.cpp
	../source/fiber_tasking_lib/fiber_abstraction.cpp
	../source/fiber_tasking_lib/read_write_lock.cpp
	../source/fiber_tasking_lib/tagged_heap.cpp
	../source/fiber_tasking_lib/tagged_heap_backed_linear_allocator.cpp
	../source/fiber_tasking_lib/task_scheduler.cpp)

add_library(FiberTaskingLib ${SOURCE_FILES})
