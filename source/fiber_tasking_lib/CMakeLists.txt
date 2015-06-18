find_package(Boost 1.56 COMPONENTS context REQUIRED)
find_package(Threads)

set(FIBER_TASKING_LIB_SRC
	basic_linear_allocator.cpp
	read_write_lock.cpp
	tagged_heap.cpp
	tagged_heap_backed_linear_allocator.cpp
	task_scheduler.cpp)

add_library(FiberTaskingLib ${FIBER_TASKING_LIB_SRC})
target_link_libraries(FiberTaskingLib EASTL ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(FiberTaskingLib PROPERTIES PREFIX "")
