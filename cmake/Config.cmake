macro(Config)
	# Determine what fiber implementation we should use
	if (MSVC)
		set(FTL_WIN32_FIBER_IMPL ON)
	else()
		set(FTL_BOOST_CONTEXT_FIBER_IMPL ON)
	endif()
	
	# Determine if the compiler supports fiber-safe TLS
	if (MSVC)
		set(FTL_FIBER_IMPL_SUPPORTS_TLS ON)
	endif()	
	
	configure_file( ${CMAKE_SOURCE_DIR}/cmake/config.h.cmake ${CMAKE_SOURCE_DIR}/include/fiber_tasking_lib/config.h )
endmacro()