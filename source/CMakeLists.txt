find_package(Boost 1.56 COMPONENTS context REQUIRED)
find_package(Threads)
include_directories(${Boost_INCLUDE_DIR})


include(SetSourceGroup)

SetSourceGroup(NAME "Core"
	SOURCE_FILES ../include/fiber_tasking_lib/global_args.h
	             ../include/fiber_tasking_lib/task_scheduler.h
	             core/task_scheduler.cpp
)

SetSourceGroup(NAME "Memory"
	SOURCE_FILES ../include/fiber_tasking_lib/basic_linear_allocator.h
	             memory/basic_linear_allocator.cpp
	             ../include/fiber_tasking_lib/tagged_heap.h
	             memory/tagged_heap.cpp
	             ../include/fiber_tasking_lib/tagged_heap_backed_linear_allocator.h
	             memory/tagged_heap_backed_linear_allocator.cpp
)

SetSourceGroup(NAME "Util"
	SOURCE_FILES ../include/fiber_tasking_lib/config.h
	             ../include/fiber_tasking_lib/fiber_abstraction.h
	             util/fiber_abstraction.cpp
	             ../include/fiber_tasking_lib/portability.h
	             ../include/fiber_tasking_lib/read_write_lock.h
	             util/read_write_lock.cpp
	             ../include/fiber_tasking_lib/thread_abstraction.h
	             ../include/fiber_tasking_lib/tls_abstraction.h
	             util/tls_abstraction.cpp
	             ../include/fiber_tasking_lib/typedefs.h
)

# Link all the sources into one
set(FIBER_TASKING_LIB_SRC
	${SRC_CORE}
	${SRC_MEMORY}
	${SRC_UTIL}
)


add_library(FiberTaskingLib ${FIBER_TASKING_LIB_SRC})
target_link_libraries(FiberTaskingLib EASTL ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
set_target_properties(FiberTaskingLib PROPERTIES PREFIX "")

set_property(TARGET FiberTaskingLib PROPERTY CXX_STANDARD_REQUIRED ON)
set_property(TARGET FiberTaskingLib PROPERTY CXX_STANDARD 11)
