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

include(AddCompilerFlags)

# Emscripten needs posix linked and enabled via command line args
if (EMSCRIPTEN)
	find_package(Threads REQUIRED)
	SET(PTHREAD_POOL_SIZE 8)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
	add_definitions(-DPTHREAD_POOL_SIZE=${PTHREAD_POOL_SIZE})
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread -s PTHREAD_POOL_SIZE=${PTHREAD_POOL_SIZE} -s ASYNCIFY")
endif()

if (EMSCRIPTEN)
    SET(CMAKE_EXECUTABLE_SUFFIX .html)
endif()

# Create an executable for each example
add_executable(ftl-example-triangle-num triangle_num.cpp)
target_link_libraries(ftl-example-triangle-num ftl)
AddCompilerFlags(ftl-example-triangle-num)
