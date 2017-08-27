cmake_minimum_required (VERSION 3.2)
project (vectrexy)

file(GLOB SRC "src/*.*")
add_executable(vectrexy ${SRC})

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(vectrexy PRIVATE _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
	target_compile_options(vectrexy PRIVATE /MP /W4 /WX)
	if (MSVC_VERSION LESS 1900) # Starting from MSVC 14 (2015), STL needs language extensions enabled
		target_compile_options(vectrexy PRIVATE /za) # disable language extensions
	else()
		target_compile_options(vectrexy PRIVATE /permissive-) # disable non-standard extensions
		target_compile_options(vectrexy PRIVATE /std:c++latest) # enable C++17 features
	endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	target_compile_options(vectrexy PRIVATE -std=c++1z)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(vectrexy PRIVATE -std=c++1z)
endif()
