cmake_minimum_required (VERSION 3.10)

project(vectrexy
	DESCRIPTION "A Vectrexy emulator by Antonio Maiorano"
	# Requires CMake 3.13
	# HOMEPAGE_URL https://github.com/amaiorano/vectrexy
)

option(BUILD_SHARED_LIBS "Build libs as shared libraries." OFF)
option(DEBUG_UI "Enable the debug UI." ON)

set(ENGINE_TYPE sdl CACHE STRING "Engine Type")
set_property(CACHE ENGINE_TYPE PROPERTY STRINGS sdl null)

string(TOLOWER ${ENGINE_TYPE} ENGINE_TYPE)
if(ENGINE_TYPE STREQUAL null)
	set(USE_NULL_ENGINE On)
elseif(ENGINE_TYPE STREQUAL sdl)
	set(USE_SDL_ENGINE On)
else()
	message(FATAL_ERROR "Unknown engine type")
endif()

if(NOT BUILD_SHARED_LIBS)
	set(BUILD_STATIC_LIBS ON)
else()
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

if (UNIX AND NOT APPLE)
	set(LINUX true)
endif()

# Force exes/libs to output in the build/ folder, rather than have each output
# in its own build/[module]/ folder. This is especially useful for shared lib
# builds so that DLLs end up in the same directory as the EXE. Note that for
# multi-config generators (VS, Xcode), sub-directories for each config will
# still be created (e.g. build/Release/, build/Debug/, etc.).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Make sure MinSizeRel and RelWithDebInfo use Release libs instead of Debug libs from vcpkg
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)

# Set module path to local cmake folder so find_package looks there first
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if(USE_NULL_ENGINE)
	add_definitions(-DENGINE_NULL)
elseif(USE_SDL_ENGINE)
	add_definitions(-DENGINE_SDL)
endif()

if(DEBUG_UI)
	add_definitions(-DDEBUG_UI_ENABLED)
endif()

# Add externals
if(LINUX)
	add_subdirectory(external/linenoise)
endif()
add_subdirectory(external/noc)

# Add libs
add_subdirectory(libs/core)
add_subdirectory(libs/engine)
add_subdirectory(libs/emulator)
add_subdirectory(libs/debugger)
if(USE_NULL_ENGINE)
	add_subdirectory(libs/null_engine)
endif()
if(USE_SDL_ENGINE)
	add_subdirectory(libs/sdl_engine)
endif()
add_subdirectory(libs/vectrexy)
