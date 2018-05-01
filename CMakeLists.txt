cmake_minimum_required (VERSION 3.2)
project (vectrexy)

# Set module path to local cmake folder so find_package looks there first
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(SDL2 REQUIRED)
find_package(SDL2_net REQUIRED)
find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLM REQUIRED)
find_package(STB REQUIRED)
find_package(Imgui REQUIRED)

# SDL2 dependent libs, required when linking against SDL2 static lib
if (MSVC)
	set(SDL2_REQUIRED_LIBS winmm.lib version.lib imm32.lib Ws2_32.lib Iphlpapi.lib)
endif()

# Enable DPI awareness on Windows 8+
if (MSVC)
	set(MANIFEST_FILE cmake/dpiawarescaleing.manifest)
endif()

# TODO: auto-gen source folder groups
file(GLOB SRC_ROOT "src/*.*")
source_group("src" FILES ${SRC_ROOT})
file(GLOB IMGUI_IMPL_SRC "src/imgui_impl/*.*")
source_group("src\\imgui_impl" FILES ${IMGUI_IMPL_SRC})
file(GLOB SHADER_SRC "src/shaders/*.*")
source_group("src\\shaders" FILES ${SHADER_SRC})
set(SRC ${SRC_ROOT} ${IMGUI_IMPL_SRC} ${SHADER_SRC})

add_executable(vectrexy ${SRC} ${MANIFEST_FILE})
include_directories(${SDL2_INCLUDE_DIR} ${SDL2_NET_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${STB_INCLUDE_PATH} ${IMGUI_INCLUDE_PATH})
target_link_libraries(vectrexy ${SDL2_LIBRARY} ${SDL2_NET_LIBRARIES} ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} ${GLEW_LIBRARIES} ${SDL2_REQUIRED_LIBS} ${IMGUI_LIBRARY})
target_compile_definitions(vectrexy PRIVATE ${GLEW_DEFINITIONS})

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
