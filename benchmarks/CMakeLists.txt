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
 
SetSourceGroup(NAME Root
	PREFIX FTL_BENCHMARK
	SOURCE_FILES main.cpp
)

SetSourceGroup(NAME Empty
	PREFIX FTL_BENCHMARK
	SOURCE_FILES empty/empty.cpp
)

SetSourceGroup(NAME "Producer Consumer"
	PREFIX FTL_BENCHMARK
	SOURCE_FILES producer_consumer/producer_consumer.cpp
)


set(FTL_BENCHMARK_SRC
	${FTL_BENCHMARK_ROOT}
	${FTL_BENCHMARK_EMPTY}
	${FTL_BENCHMARK_PRODUCER_CONSUMER}
)


add_executable(ftl-benchmark ${FTL_BENCHMARK_SRC})
target_link_libraries(ftl-benchmark ftl nonius)
