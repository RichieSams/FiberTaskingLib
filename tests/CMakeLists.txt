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


SetSourceGroup(NAME "Fiber Abstraction"
	PREFIX FTL_TEST
	SOURCE_FILES fiber_abstraction/single_fiber_switch.cpp
	             fiber_abstraction/nested_fiber_switch.cpp
	             fiber_abstraction/floating_point_fiber_switch.cpp
)

SetSourceGroup(NAME "Functional"
	PREFIX FTL_TEST 
    SOURCE_FILES functional/calc_triangle_num.cpp
                 functional/producer_consumer.cpp
)

SetSourceGroup(NAME "Utilities"
	PREFIX FTL_TEST
	SOURCE_FILES utilities/event_callbacks.cpp
                 utilities/fibtex.cpp
	             utilities/thread_local.cpp
)

SetSourceGroup(NAME "Root"
	PREFIX FTL_TEST
	SOURCE_FILES main.cpp
)

# Link all the sources into one
set(FIBER_TASKING_LIB_TESTS_SRC
	${FTL_TEST_FIBER_ABSTRACTION}
	${FTL_TEST_FUNCTIONAL}
	${FTL_TEST_UTILITIES}
    ${FTL_TEST_ROOT}
)


add_executable(ftl-test ${FIBER_TASKING_LIB_TESTS_SRC})
target_link_libraries(ftl-test catch2 ftl)

# Set the warnings
AddCompilerFlags(ftl-test)
