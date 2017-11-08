cmake_minimum_required (VERSION 3.2)
project (vectrexy)

# Set module path to local cmake folder so find_package looks there first
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLM REQUIRED)
find_package(PNG REQUIRED)

# SDL2 dependent libs, required when linking against SDL2 static lib
if (MSVC)
	set(SDL2_REQUIRED_LIBS winmm.lib version.lib imm32.lib)
endif()

file(GLOB SRC "src/*.*")
source_group("Source" FILES ${SRC})
file(GLOB SHADER_SRC "shaders/*.*")
source_group("Shaders" FILES ${SHADER_SRC})

add_executable(vectrexy ${SRC} ${SHADER_SRC})
include_directories(${SDL2_INCLUDE_DIR} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${PNG_INCLUDE_DIRS})
target_link_libraries(vectrexy ${SDL2_LIBRARY} ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} ${GLEW_LIBRARIES} ${SDL2_REQUIRED_LIBS} ${PNG_LIBRARIES})
target_compile_definitions(vectrexy PRIVATE ${GLEW_DEFINITIONS} ${PNG_DEFINITIONS})

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
