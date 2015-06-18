set(EASTL_SRC
	src/allocator.cpp
    src/assert.cpp
    src/fixed_pool.cpp
    src/hashtable.cpp
    src/red_black_tree.cpp
    src/string.cpp)

add_library(EASTL ${EASTL_SRC})
set_target_properties(EASTL PROPERTIES PREFIX "")
