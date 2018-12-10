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
	Check_And_Add_Flag(-Wno-unknown-pragmas)
	Check_And_Add_Flag(-Wuseless-cast)
else()
	Check_And_Add_Flag(/W4)
endif()

SetSourceGroup(NAME Core
	PREFIX FTL
	SOURCE_FILES 
				 ../include/ftl/task.h
	             ../include/ftl/atomic_counter.h
	             atomic_counter.cpp
	             ../include/ftl/task_scheduler.h
				 ../include/ftl/typedefs.h
	             task_scheduler.cpp
)

SetSourceGroup(NAME Util
	PREFIX FTL
	SOURCE_FILES ../include/ftl/assert.h
	             ../include/ftl/config.h
	             ../include/ftl/fiber.h
	             ../include/ftl/thread_abstraction.h
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
target_link_libraries(ftl boost_context ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(ftl PUBLIC ../include)

# Remove the prefix
set_target_properties(ftl PROPERTIES PREFIX "")
