# EASTL
add_subdirectory(EASTL)
set_target_properties (EASTL PROPERTIES FOLDER third_party)

# GTest
if (FTL_BUILD_TESTS)
	SET(gtest_force_shared_crt ON CACHE BOOL "Use the dll runtime" )
	add_subdirectory(gtest)
	set_target_properties (gtest PROPERTIES FOLDER third_party)
	set_target_properties (gtest_main PROPERTIES FOLDER third_party)
endif()
