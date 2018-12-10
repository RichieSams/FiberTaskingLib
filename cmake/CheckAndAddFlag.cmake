include(CheckCXXCompilerFlag)

# Function to add flag if compiler supports it
function(Check_And_Add_Flag)
	# Sanitize flag to become cmake variable name
	string (REPLACE "-" "_" NAME ${ARGV0})
	string (SUBSTRING ${NAME} 1 -1 NAME)
	string (FIND ${NAME} "=" EQUALS)
	string (SUBSTRING ${NAME} 0 ${EQUALS} NAME)
	CHECK_CXX_COMPILER_FLAG(${ARGV0} ${NAME})
	if (${NAME}) 
		add_compile_options(${ARGV0})
	endif()
endfunction()
