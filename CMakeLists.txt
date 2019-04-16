cmake_minimum_required (VERSION 3.13)

project (vectrexy
	DESCRIPTION "A Vectrexy emulator by Antonio Maiorano"
	HOMEPAGE_URL https://github.com/amaiorano/vectrexy
)

option (BUILD_SHARED_LIBS "Build libs as shared libraries." OFF)

if (NOT BUILD_SHARED_LIBS)
  set (BUILD_STATIC_LIBS ON)
else()
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# Make sure MinSizeRel and RelWithDebInfo use Release libs instead of Debug libs from vcpkg
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)

# Set module path to local cmake folder so find_package looks there first
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# core library
if (UNIX AND NOT APPLE)
	set(LINUX true)
endif()

file (GLOB_RECURSE CORE_SRC_FILES "libs/core/include/*.*" "libs/core/src/*.*")
source_group(TREE "../libs/core" FILES ${CORE_SRC_FILES})

file(GLOB THIRD_PARTY_NOC "libs/core/external/noc/noc_file_dialog.h")
source_group(TREE "../libs/core" FILES ${THIRD_PARTY_NOC})

if (LINUX)
	file(GLOB THIRD_PARTY_LINENOISE "libs/core/external/linenoise/linenoise.*")
	source_group(TREE "../libs/core" FILES ${THIRD_PARTY_LINENOISE})
endif()

add_library(core ${CORE_SRC_FILES} ${THIRD_PARTY_NOC} ${THIRD_PARTY_LINENOISE})

find_package(SDL2 CONFIG REQUIRED)
find_package(sdl2-net CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(STB MODULE REQUIRED)
if (LINUX)
	find_package(GTK2 2.4 REQUIRED)
	target_include_directories(core PRIVATE ${GTK2_INCLUDE_DIRS})
	target_link_libraries(core PRIVATE ${GTK2_LIBRARIES})
endif()

target_include_directories(core
	PUBLIC "libs/core/include"
	PRIVATE ${STB_INCLUDE_PATH} "libs/core/external"
)
target_compile_features(core PUBLIC cxx_std_17)
set_target_properties(core PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(core PUBLIC
	SDL2::SDL2_net
	SDL2::SDL2-static
	imgui::imgui
)

# TODO: Move this out of Core and make it something each module can include
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(core PUBLIC _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
	target_compile_options(core PUBLIC /MP /W4 /WX)
		# By default, CMake uses dynamic CRT, which is good if we're building DLLs to avoid
		# issues with allocations in one DLL trying to be freed in another. But when building
		# statically, force using the static CRT so we don't have to worry about user having
		# the right CRT installed.
		if (NOT BUILD_SHARED_LIBS)
			target_compile_options(core PUBLIC "$<$<CONFIG:Debug>:/MTd>")
			target_compile_options(core PUBLIC "$<$<CONFIG:Release>:/MT>")
			target_compile_options(core PUBLIC "$<$<CONFIG:MinSizeRel>:/MT>")
			target_compile_options(core PUBLIC "$<$<CONFIG:RelWithDebInfo>:/MT>")
		endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# ...
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(core PUBLIC -Wno-format-security) # todo: remove this and fix warnings
	target_link_libraries(core PUBLIC stdc++fs) # required for filesystem
	endif()

# emulator library
file (GLOB_RECURSE EMULATOR_SRC_FILES "libs/emulator/include/*.*" "libs/emulator/src/*.*")
source_group(TREE "../libs/emulator" FILES ${EMULATOR_SRC_FILES})
add_library(emulator ${EMULATOR_SRC_FILES})
target_include_directories(emulator PUBLIC "libs/emulator/include")
target_link_libraries(emulator PUBLIC core)

# debugger library
file (GLOB_RECURSE DEBUGGER_SRC_FILES "libs/debugger/include/*.*" "libs/debugger/src/*.*")
source_group(TREE "../libs/debugger" FILES ${DEBUGGER_SRC_FILES})
add_library(debugger ${DEBUGGER_SRC_FILES})
target_include_directories(debugger PUBLIC "libs/debugger/include")
target_link_libraries(debugger PUBLIC core emulator)

# engine library
file (GLOB_RECURSE ENGINE_SRC_FILES "libs/engine/include/*.*" "libs/engine/src/*.*")
source_group(TREE "../libs/engine" FILES ${ENGINE_SRC_FILES})
add_library(engine ${ENGINE_SRC_FILES})
target_include_directories(engine PUBLIC "libs/engine/include")
target_link_libraries(engine PUBLIC core emulator)

# sdl_gl_engine library

# Force linking against GLEW static libs, even when BUILD_SHARED_LIBS is ON
set(GLEW_USE_STATIC_LIBS ON)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL MODULE REQUIRED)

file (GLOB_RECURSE SDL_GL_ENGINE_SRC_FILES "libs/sdl_gl_engine/include/*.*" "libs/sdl_gl_engine/src/*.*")
source_group(TREE "../libs/sdl_gl_engine" FILES ${SDL_GL_ENGINE_SRC_FILES})
add_library(sdl_gl_engine ${SDL_GL_ENGINE_SRC_FILES})
target_include_directories(sdl_gl_engine PUBLIC "libs/sdl_gl_engine/include")
target_link_libraries(sdl_gl_engine
	PUBLIC
		core
		emulator
		debugger
		engine
		SDL2::SDL2main

	PRIVATE
		SDL2::SDL2-static
		imgui::imgui
		GLEW::GLEW
		glm		
		${OPENGL_gl_LIBRARY}
		${OPENGL_glu_LIBRARY}
)

# vectrexy exe
file (GLOB_RECURSE VECTREXY_SRC_FILES "libs/vectrexy/include/*.*" "libs/vectrexy/src/*.*")
source_group(TREE "../libs/vectrexy" FILES ${VECTREXY_SRC_FILES})

# Enable DPI awareness on Windows 8+
if (MSVC)
	set(MANIFEST_FILE cmake/dpiawarescaleing.manifest)
endif()

add_executable(vectrexy ${VECTREXY_SRC_FILES} ${MANIFEST_FILE})

target_link_libraries(vectrexy PRIVATE
	core
	emulator
	debugger
	engine
	sdl_gl_engine
)
