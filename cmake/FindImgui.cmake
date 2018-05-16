#
# Try to find imgui library and include path.
# Once done this will define
#
# IMGUI_FOUND
# IMGUI_INCLUDE_PATH
# IMGUI_LIBRARY
# 

SET(IMGUI_SEARCH_PATHS
	$ENV{IMGUI_ROOT}
	${DEPENDENCIES_ROOT}
	/usr/local
	/usr)

FIND_PATH(IMGUI_INCLUDE_PATH
    NAMES
        imgui/imgui.h
		imgui.h
    PATHS
        ${IMGUI_SEARCH_PATHS}
    PATH_SUFFIXES
        include
    DOC
        "The directory where imgui/imgui.h resides"
)

FIND_LIBRARY(IMGUI_LIBRARY
    NAMES
       imgui.lib
       imgui
    PATHS
        ${IMGUI_SEARCH_PATHS}
    PATH_SUFFIXES
        lib
    DOC
        "The directory where imgui.lib resides"
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(IMGUI DEFAULT_MSG IMGUI_LIBRARY IMGUI_INCLUDE_PATH)
