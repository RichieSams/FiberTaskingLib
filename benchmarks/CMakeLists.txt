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
 

set(FTL_BENCHMARK_SRC
	empty/empty.cpp
	producer_consumer/producer_consumer.cpp
)

# Set the c++ std
if (FTL_CPP_17)
	target_compile_features(ftl PUBLIC cxx_std_17)
	add_definitions(-DFTL_CPP_17=1)
else()
	target_compile_features(ftl PUBLIC cxx_std_11)
endif()

add_executable(ftl-benchmark ${FTL_BENCHMARK_SRC})
target_link_libraries(ftl-benchmark ftl Catch2::Catch2WithMain)
