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

# Catch2
if (FTL_BUILD_TESTS)
	add_subdirectory(catch2)
endif()


# Boost Context

# CMake fails to generate the correct ninja files for MASM
# https://gitlab.kitware.com/cmake/cmake/issues/18889
# We hack a fix for this by manually setting the MASM compile options
if (WIN32)
	# Save the original flags
	set(SAVED_LINKER_FLAGS ${CMAKE_STATIC_LINKER_FLAGS})
	set(CMAKE_STATIC_LINKER_FLAGS "")

    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
        set(CMAKE_AR ${CMAKE_ASM_COMPILER_AR})
    elseif(MSVC)
        set(CMAKE_AR lib.exe)
        set(CMAKE_ASM_MASM_COMPILE_OPTIONS_MSVC_RUNTIME_LIBRARY_MultiThreaded         "")
        set(CMAKE_ASM_MASM_COMPILE_OPTIONS_MSVC_RUNTIME_LIBRARY_MultiThreadedDLL      "")
        set(CMAKE_ASM_MASM_COMPILE_OPTIONS_MSVC_RUNTIME_LIBRARY_MultiThreadedDebug    "")
        set(CMAKE_ASM_MASM_COMPILE_OPTIONS_MSVC_RUNTIME_LIBRARY_MultiThreadedDebugDLL "")
        set(CMAKE_ASM_MASM_CREATE_STATIC_LIBRARY "<CMAKE_AR> /OUT:<TARGET> <LINK_FLAGS> <OBJECTS>")
    endif()
endif()
add_subdirectory(boost_context)
set_target_properties(boost_context PROPERTIES FOLDER third_party)

if (WIN32)
	# Restore the original flags
	set(CMAKE_STATIC_LINKER_FLAGS ${SAVED_LINKER_FLAGS})
endif()


# Nonius
add_subdirectory(nonius)
