#
# Try to find STB include path.
# Once done this will define
#
# STB_FOUND
# STB_INCLUDE_PATH
# 

SET(STB_SEARCH_PATHS
	$ENV{STB_ROOT}
	${DEPENDENCIES_ROOT}
	/usr/local
	/usr)

FIND_PATH(STB_INCLUDE_PATH
    NAMES
		stb.h
    PATHS
        ${STB_SEARCH_PATHS}
    PATH_SUFFIXES
        include
    DOC
        "The directory where stb.h resides"
)

SET(STB_FOUND "NO")
IF (STB_INCLUDE_PATH)
    SET(STB_FOUND "YES")
    #message("EXTERNAL LIBRARY 'STB' FOUND")
ELSE()
    message("ERROR: EXTERNAL LIBRARY 'STB' NOT FOUND")
ENDIF (STB_INCLUDE_PATH)
