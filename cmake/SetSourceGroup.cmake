# Copyright Adrian Astley 2016

include(CMakeParseArguments)

macro( SetSourceGroup )
	set(oneValueArgs NAME PREFIX)
	set(multiValuesArgs SOURCE_FILES)
	cmake_parse_arguments(SRC_GROUP "" "${oneValueArgs}" "${multiValuesArgs}" ${ARGN} )
	
	string(REPLACE " " "_" VAR_SUFFIX "${SRC_GROUP_NAME}")
	string(REPLACE "/" "_" VAR_SUFFIX "${VAR_SUFFIX}")
	string(TOUPPER ${VAR_SUFFIX} VAR_SUFFIX)
	
	set(VAR_NAME ${SRC_GROUP_PREFIX}_${VAR_SUFFIX})
	
	set(${VAR_NAME}
	    ${SRC_GROUP_SOURCE_FILES}
	)
	
	string(REPLACE "Root" " " SRC_GROUP_NAME "${SRC_GROUP_NAME}")
	string(REPLACE "root" " " SRC_GROUP_NAME "${SRC_GROUP_NAME}")
	string(REPLACE "/" "\\" SRC_GROUP_NAME "${SRC_GROUP_NAME}")

	source_group("${SRC_GROUP_NAME}" FILES ${${VAR_NAME}})
endmacro()
