enable_testing()
include(GTestAddTests)


include(SetSourceGroup)

SetSourceGroup(NAME "Root"
	SOURCE_FILES main.cpp
)

SetSourceGroup(NAME "Common"
	SOURCE_FILES common/eastl_defaults.cpp
)

SetSourceGroup(NAME "Fiber Abstraction"
	SOURCE_FILES fiber_abstraction/nested_fiber_switch.cpp
	             fiber_abstraction/single_fiber_switch.cpp
)

SetSourceGroup(NAME "Maze"
	SOURCE_FILES maze/maze.cpp
	maze/maze10x10.h
)

SetSourceGroup(NAME "Producer Consumer"
	SOURCE_FILES producer_consumer/producer_consumer.cpp
)

SetSourceGroup(NAME "TLS Abstraction"
	SOURCE_FILES tls_abstraction/tls_abstraction.cpp
)

SetSourceGroup(NAME "Triangle Number"
	SOURCE_FILES triangle_number/calc_triangle_num.cpp
)


# Link all the sources into one
set(FIBER_TASKING_LIB_TESTS_SRC
	${SRC_ROOT}
	${SRC_COMMON}
	${SRC_FIBER_ABSTRACTION}
	${SRC_MAZE}
	${SRC_PRODUCER_CONSUMER}
	${SRC_TLS_ABSTRACTION}
	${SRC_TRIANGLE_NUMBER}
)


add_executable(FiberTaskingLib-test ${FIBER_TASKING_LIB_TESTS_SRC})
target_link_libraries(FiberTaskingLib-test gtest gtest_main FiberTaskingLib)

GTEST_ADD_TESTS(FiberTaskingLib-test "" ${FIBER_TASKING_LIB_TESTS_SRC})
