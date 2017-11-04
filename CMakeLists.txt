cmake_minimum_required (VERSION 3.2)
project (vectrexy)

# To find SDL2 using find_package below, we set the module path to a directory containing
# the FindSDL2.cmake module script.
# NOTE: On Windows, set "SDL2" environment variable to the path to the SDL2 development libraries root
#       which can be downloaded here: https://www.libsdl.org/download-2.0.php

# Set module path to local cmake folder so find_package looks there first
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# On Windows, set "SDL2" environment variable to the path to the SDL2 development libraries root
# which can be downloaded here: https://www.libsdl.org/download-2.0.php
find_package(SDL2 REQUIRED)
# OpenGL is usually available on all systems
find_package(OpenGL REQUIRED)
# Download GLEW and pass -DGLEW_ROOT_DIR=path/to/glew to CMake.
# Optionally pass -DGLEW_USE_STATIC_LIBS=1 to link against static lib.
find_package(GLEW REQUIRED)
# Download GLM and pass -GLM_ROOT_DIR=path/to/glm to CMake.
find_package(GLM REQUIRED)

# SDL2 dependent libs, required when linking against SDL2 static lib
if (MSVC)
	set(SDL2_REQUIRED_LIBS winmm.lib version.lib imm32.lib)
endif()

file(GLOB SRC "src/*.*")
source_group("Source" FILES ${SRC})
file(GLOB SHADER_SRC "shaders/*.*")
source_group("Shaders" FILES ${SHADER_SRC})

add_executable(vectrexy ${SRC} ${SHADER_SRC})
include_directories(${SDL2_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
target_link_libraries(vectrexy ${SDL2_LIBRARY} ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} ${GLEW_LIBRARIES} ${SDL2_REQUIRED_LIBS})
target_compile_definitions(vectrexy PRIVATE ${GLEW_DEFINITIONS})

# NOTE: Removing this as vcpkg already adds its own post-build copy step for dlls
# For VS, add post-build step to copy SDL2.dll to the output directory
#if(MSVC)
#	add_custom_command(TARGET vectrexy POST_BUILD
#						COMMAND ${CMAKE_COMMAND} -E copy_if_different
#						"${SDL2_INCLUDE_DIR}/../lib/x86/SDL2.DLL" $<TARGET_FILE_DIR:vectrexy>)
#endif()

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
