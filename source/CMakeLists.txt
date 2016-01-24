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

if (FTL_BOOST_CONTEXT_FIBER_IMPL)
	find_package(Boost 1.56 COMPONENTS context REQUIRED)
	find_package(Threads)
	include_directories(${Boost_INCLUDE_DIR})
endif()

include(SetSourceGroup)

SetSourceGroup(NAME "Core"
	SOURCE_FILES ../include/fiber_tasking_lib/config.h
	             ../include/fiber_tasking_lib/global_args.h
				 ../include/fiber_tasking_lib/task.h
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
	SOURCE_FILES ../include/fiber_tasking_lib/fiber_abstraction.h
	             util/fiber_abstraction.cpp
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
target_link_libraries(FiberTaskingLib EASTL)

if (FTL_BOOST_CONTEXT_FIBER_IMPL)
	target_link_libraries(FiberTaskingLib ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

set_target_properties(FiberTaskingLib PROPERTIES PREFIX "")
