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

include(SetSourceGroup)

# Unix systems need to be explicitly linked to the threading lib
if (UNIX)
	find_package(Threads REQUIRED)
endif()


# Determine the target architecture
#
# Taken from Richard Maxwell's 'sewing' library: https://github.com/JodiTheTigger/sewing
#
# Enable the assemblers
if (WIN32 AND MSVC)
	enable_language(ASM_MASM)
else()
	enable_language(ASM)
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
	set(FTL_PLATFORM "x86_64")
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
	set(FTL_PLATFORM "x86_64")
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "i386")
	set(FTL_PLATFORM "i386")
endif()

if (FTL_PLATFORM STREQUAL "x86_64")
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "4")
		set(FTL_PLATFORM "i386")
	endif()
endif()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
	if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
		set(FTL_PLATFORM "arm64")
	else()
		set(FTL_PLATFORM "arm")
	endif()

	set(FTL_OS  "aapcs")
endif()

if (WIN32)
	# arm requires aapcs. So we have to check if arm already set the calling convention
	if (NOT FTL_OS)
		set(FTL_OS  "ms")
	endif()
	set(FTL_ABI "pe")

	if (MSVC)
		set(FTL_ASM "masm.asm")
	else()
		set(FTL_ASM "gas.S")
	endif()
elseif(APPLE)
	set(FTL_OS  "sysv")
	set(FTL_ABI "macho")
	set(FTL_ASM "gas.S")
elseif(UNIX)
	# arm requires aapcs. So we have to check if arm already set the calling convention
	if (NOT FTL_OS)
		set(FTL_OS  "sysv")
	endif()
	set(FTL_ABI "elf")
	set(FTL_ASM "gas.S")
endif()

message(STATUS "-- System processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "-- Target processor: ${FTL_PLATFORM}")
message(STATUS "-- Target asm      : ${FTL_PLATFORM}-${FTL_OS}-${FTL_ABI}-${FTL_ASM}")



if ((FTL_PLATFORM STREQUAL "x86_64") OR (FTL_PLATFORM STREQUAL "i386"))
	add_definitions(-DFTL_STRONG_MEMORY_MODEL=1)
endif()


SetSourceGroup(NAME Core
	PREFIX FTL
	SOURCE_FILES 
				 ../include/fiber_tasking_lib/task.h
	             ../include/fiber_tasking_lib/task_scheduler.h
				 ../include/fiber_tasking_lib/typedefs.h
	             task_scheduler.cpp
)

SetSourceGroup(NAME Util
	PREFIX FTL
	SOURCE_FILES ../include/fiber_tasking_lib/config.h
	             ../include/fiber_tasking_lib/thread_abstraction.h
	             ../include/fiber_tasking_lib/wait_free_queue.h
)

SetSourceGroup(NAME Fiber
	PREFIX FTL
	SOURCE_FILES ../include/fiber_tasking_lib/fiber.h
	             asm/make_${FTL_PLATFORM}_${FTL_OS}_${FTL_ABI}_${FTL_ASM}
	             asm/jump_${FTL_PLATFORM}_${FTL_OS}_${FTL_ABI}_${FTL_ASM})

if (FTL_ASM MATCHES "masm.asm" AND FTL_PLATFORM MATCHES "i386")
	set_source_files_properties(asm/make_${FTL_PLATFORM}_${FTL_OS}_${FTL_ABI}_${FTL_ASM}
	                            asm/jump_${FTL_PLATFORM}_${FTL_OS}_${FTL_ABI}_${FTL_ASM}
	                            PROPERTIES
	                            COMPILE_FLAGS "/safeseh")
endif()

# Link all the sources into one
set(FIBER_TASKING_LIB_SRC
	${FTL_CORE}
	${FTL_UTIL}
	${FTL_FIBER}
)

add_library(FiberTaskingLib STATIC ${FIBER_TASKING_LIB_SRC})
target_link_libraries(FiberTaskingLib ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(FiberTaskingLib PUBLIC ../include)

# Remove the prefix
set_target_properties(FiberTaskingLib PROPERTIES PREFIX "")
