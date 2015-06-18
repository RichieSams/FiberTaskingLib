set(EXAMPLE_PROJECT_SRC
	eastl_defaults.cpp
	main.cpp)

add_executable(ExampleProject ${EXAMPLE_PROJECT_SRC})
target_link_libraries(ExampleProject FiberTaskingLib)