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
    PATHS
        ${IMGUI_SEARCH_PATHS}
    PATH_SUFFIXES
        lib
    DOC
        "The directory where imgui.lib resides"
)

SET(IMGUI_FOUND "NO")
IF (IMGUI_INCLUDE_PATH AND IMGUI_LIBRARY)
    SET(IMGUI_LIBRARIES ${IMGUI_LIBRARY})
    SET(IMGUI_FOUND "YES")
    #message("EXTERNAL LIBRARY 'IMGUI' FOUND")
ELSE()
    message("ERROR: EXTERNAL LIBRARY 'IMGUI' NOT FOUND")
ENDIF (IMGUI_INCLUDE_PATH AND IMGUI_LIBRARY)
