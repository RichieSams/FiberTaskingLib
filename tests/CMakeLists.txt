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

include(GTestAddTests)
include(SetSourceGroup)


SetSourceGroup(NAME Root
	PREFIX FTL_TEST
	SOURCE_FILES main.cpp
)

SetSourceGroup(NAME "Fiber Abstraction"
	PREFIX FTL_TEST
	SOURCE_FILES fiber_abstraction/single_fiber_switch.cpp
	             fiber_abstraction/nested_fiber_switch.cpp
)

SetSourceGroup(NAME "Producer Consumer"
	PREFIX FTL_TEST
	SOURCE_FILES producer_consumer/producer_consumer.cpp
)

SetSourceGroup(NAME "Triangle Number"
	PREFIX FTL_TEST
	SOURCE_FILES triangle_number/calc_triangle_num.cpp
)


# Link all the sources into one
set(FIBER_TASKING_LIB_TESTS_SRC
	${FTL_TEST_ROOT}
	${FTL_TEST_FIBER_ABSTRACTION}
	${FTL_TEST_PRODUCER_CONSUMER}
	${FTL_TEST_TRIANGLE_NUMBER}
)


add_executable(FiberTaskingLib-test ${FIBER_TASKING_LIB_TESTS_SRC})
target_link_libraries(FiberTaskingLib-test gtest gtest_main FiberTaskingLib)

GTEST_ADD_TESTS(FiberTaskingLib-test "" ${FIBER_TASKING_LIB_TESTS_SRC})
