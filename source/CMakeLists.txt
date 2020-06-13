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

include(SetSourceGroup)
include(AddCompilerFlags)


# Unix systems need to be explicitly linked to the threading lib
if (UNIX)
	find_package(Threads REQUIRED)
endif()

# Figure out the processor architecture
if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(FTL_ARCH "x86_64")
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
	set(FTL_ARCH "x86_64")
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "i386")
	set(FTL_ARCH "i386")
endif()

if (FTL_ARCH STREQUAL "x86_64")
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
		set(FTL_ARCH "i386")
	endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(FTL_ARCH "arm64")
	else()
		set(FTL_ARCH "arm")
	endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
	set(FTL_ARCH "arm64")
endif()

if ((FTL_ARCH STREQUAL "x86_64") OR (FTL_ARCH STREQUAL "i386"))
	add_definitions(-DFTL_STRONG_MEMORY_MODEL=1)
endif()

SetSourceGroup(NAME Core
	PREFIX FTL
	SOURCE_FILES 
				 ../include/ftl/task.h
	             ../include/ftl/base_counter.h
	             base_counter.cpp
	             ../include/ftl/atomic_counter.h
	             ../include/ftl/task_counter.h
	             ../include/ftl/task_scheduler.h
	             task_scheduler.cpp
)

SetSourceGroup(NAME Util
	PREFIX FTL
	SOURCE_FILES ../include/ftl/assert.h
	             ../include/ftl/config.h
	             ../include/ftl/fiber.h
	             ../include/ftl/fibtex.h
	             ../include/ftl/thread_abstraction.h
	             thread_abstraction.cpp
	             ../include/ftl/thread_local.h
	             ../include/ftl/wait_free_queue.h
				 ../include/ftl/ftl_valgrind.h
)

# Link all the sources into one
set(FIBER_TASKING_LIB_SRC
	${FTL_CORE}
	${FTL_UTIL}
)

add_library(ftl STATIC ${FIBER_TASKING_LIB_SRC})
target_compile_options(ftl PUBLIC -DNUM_WAITING_FIBER_SLOTS=${FTL_NUM_WAITING_FIBER_SLOTS})
target_include_directories(ftl PUBLIC ../include)
target_link_libraries(ftl boost_context ${CMAKE_THREAD_LIBS_INIT})

# Remove the prefix
set_target_properties(ftl PROPERTIES PREFIX "")

# Set the c++ std
if (FTL_CPP_17)
	target_compile_features(ftl PUBLIC cxx_std_17)
else()
	target_compile_features(ftl PUBLIC cxx_std_11)
endif()

# Set the warnings
AddCompilerFlags(ftl)
