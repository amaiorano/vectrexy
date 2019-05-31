cmake_minimum_required (VERSION 3.10)

# Try to find vcpkg toolchain file in sibling directory as a convenience
get_filename_component(VCPKG_TOOLCHAIN_FILE "../vcpkg/scripts/buildsystems/vcpkg.cmake" ABSOLUTE)
if(NOT CMAKE_TOOLCHAIN_FILE AND EXISTS ${VCPKG_TOOLCHAIN_FILE})
	set(CMAKE_TOOLCHAIN_FILE ${VCPKG_TOOLCHAIN_FILE})
	message("Auto setting CMAKE_TOOLCHAIN_FILE to ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(NOT VCPKG_TARGET_TRIPLET AND WIN32)
	set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)
	message("Auto setting VCPKG_TARGET_TRIPLET to ${VCPKG_TARGET_TRIPLET}")
endif()

if(NOT CMAKE_GENERATOR_PLATFORM AND WIN32)
	set(CMAKE_GENERATOR_PLATFORM "x64" CACHE STRING "" FORCE)
	message("Auto setting CMAKE_GENERATOR_PLATFORM to ${CMAKE_GENERATOR_PLATFORM}")
endif()

project(vectrexy
	DESCRIPTION "A Vectrexy emulator by Antonio Maiorano"
	# Requires CMake 3.13
	# HOMEPAGE_URL https://github.com/amaiorano/vectrexy
)

option(BUILD_SHARED_LIBS "Build libs as shared libraries." OFF)
option(USE_SDL_ENGINE "Use the SDL engine." ON)

if(NOT BUILD_SHARED_LIBS)
  set(BUILD_STATIC_LIBS ON)
else()
	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
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

if(USE_SDL_ENGINE)
	add_definitions(-DPLATFORM_SDL)
endif()

add_subdirectory(libs/core)
add_subdirectory(libs/engine)
add_subdirectory(libs/emulator)
add_subdirectory(libs/debugger)
if(USE_SDL_ENGINE)
	add_subdirectory(libs/sdl_engine)
endif()
add_subdirectory(libs/vectrexy)
