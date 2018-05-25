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

if (UNIX AND NOT APPLE)
	set(LINUX true)
endif()

if (LINUX)
	find_package(GTK2 2.4 REQUIRED gtk)
	set(EXTRA_INCLUDE_DIRS ${EXTRA_INCLUDE_DIRS} ${GTK2_INCLUDE_DIRS})
	set(EXTRA_LIBS ${EXTRA_LIBS} ${GTK2_LIBRARIES}) # required for filesystem
endif()

# SDL2 dependent libs, required when linking against SDL2 static lib
if (MSVC)
	set(SDL2_REQUIRED_LIBS winmm.lib version.lib imm32.lib Ws2_32.lib Iphlpapi.lib)
elseif (LINUX)
	set(SDL2_REQUIRED_LIBS dl sndio)
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
file(GLOB THIRD_PARTY_NOC "thirdparty/noc/noc_file_dialog.h")
source_group("thirdparty\\noc" FILES ${THIRD_PARTY_NOC})
set(SRC ${SRC_ROOT} ${IMGUI_IMPL_SRC} ${SHADER_SRC} ${THIRD_PARTY_NOC})
if (LINUX)
	file(GLOB THIRD_PARTY_NOC "thirdparty/linenoise/linenoise.*")
	source_group("thirdparty\\linenoise" FILES ${THIRD_PARTY_LINENOISE})
endif()
set(SRC ${SRC_ROOT} ${IMGUI_IMPL_SRC} ${SHADER_SRC} ${THIRD_PARTY_NOC} ${THIRD_PARTY_LINENOISE})

# Add other include directories
set(EXTRA_INCLUDE_DIRS ${EXTRA_INCLUDE_DIRS} "thirdparty")

add_executable(vectrexy ${SRC} ${MANIFEST_FILE})

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
	target_compile_options(vectrexy PRIVATE -Wno-format-security) # todo: remove this and fix warnings
	set(EXTRA_LIBS ${EXTRA_LIBS} stdc++fs) # required for filesystem
endif()

include_directories(${SDL2_INCLUDE_DIR} ${SDL2_NET_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${STB_INCLUDE_PATH} ${IMGUI_INCLUDE_PATH} ${EXTRA_INCLUDE_DIRS})
target_link_libraries(vectrexy ${SDL2_LIBRARY} ${SDL2_NET_LIBRARIES} ${OPENGL_gl_LIBRARY} ${OPENGL_glu_LIBRARY} ${GLEW_LIBRARIES} ${SDL2_REQUIRED_LIBS} ${IMGUI_LIBRARY} ${EXTRA_LIBS})
target_compile_definitions(vectrexy PRIVATE ${GLEW_DEFINITIONS})
