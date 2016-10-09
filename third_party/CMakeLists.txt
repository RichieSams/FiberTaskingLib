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

# GTest
if (FTL_BUILD_TESTS)
	SET(gtest_force_shared_crt ON CACHE BOOL "Use the dll runtime" )
	add_subdirectory(gtest)
	set_target_properties(gtest PROPERTIES FOLDER third_party)
	set_target_properties(gtest_main PROPERTIES FOLDER third_party)
endif()

# Boost Context
add_subdirectory(boost_context)
set_target_properties(boost_context PROPERTIES FOLDER third_party)
