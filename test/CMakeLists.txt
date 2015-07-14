find_package(GTest QUIET)

set (TEST_PROJECT_SRC
	calc_triangle_num.cpp
	eastl_defaults.cpp
	main.cpp
	maze.cpp
	nested_fiber_switch.cpp
	producer_consumer.cpp
	single_fiber_switch.cpp
	tls_abstraction.cpp
)

add_executable(FiberTaskingLib-test ${TEST_PROJECT_SRC})
target_link_libraries(FiberTaskingLib-test gtest gtest_main FiberTaskingLib)

GTEST_ADD_TESTS(FiberTaskingLib-test "" ${TEST_PROJECT_SRC})
