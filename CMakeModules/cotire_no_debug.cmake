# - cotire (compile time reducer, not enabled at debug)
#
# See the cotire manual for usage hints.

string(TOUPPER ${CMAKE_BUILD_TYPE} build_type)

if ("${build_type}" STREQUAL "DEBUG")
	function (cotire)
	endfunction(cotire)
else()
	include(cotire)
endif()
