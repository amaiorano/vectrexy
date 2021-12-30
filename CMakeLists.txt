cmake_minimum_required (VERSION 3.15)

# Disable adding MSVC CRT to CMAKE_<LANG>_FLAGS_<CONFIG> by default.
cmake_policy(SET CMP0091 NEW)

# Disable adding MSVC warning flags to CMAKE_<LANG>_FLAGS by default.
cmake_policy(SET CMP0092 NEW)

option(BUILD_SHARED_LIBS "Build libs as shared libraries." OFF)
option(DEBUG_UI "Enable the debug UI." ON)
option(BUILD_TESTS "Build tests." OFF)

# Attempt to set VCPKG_TARGET_TRIPLET on Windows to use static libs.
# This is currently required by certain packages like ImGui.
if(WIN32 AND NOT VCPKG_TARGET_TRIPLET)
	set(VCPKG_ARCH x64) # Default to x64
	# VS's open folder/CMake support sets CMAKE_INSTALL_PREFIX
	if(CMAKE_INSTALL_PREFIX MATCHES "x86-")
		set(VCPKG_ARCH x86)
	# Otherwise see if user set the arch via -A
	elseif(CMAKE_GENERATOR_PLATFORM STREQUAL Win32)
		set(VCPKG_ARCH x86)
	endif()
	set(VCPKG_TARGET_TRIPLET ${VCPKG_ARCH}-windows-static)
	unset(VCPKG_ARCH)
	message(STATUS "VCPKG_TARGET_TRIPLET set to: ${VCPKG_TARGET_TRIPLET}")
endif()

# Setup vcpkg manifest mode. This will automatically bootstrap and download/update
# packages listed in vcpkg.json.
# https://github.com/microsoft/vcpkg/blob/master/docs/users/manifests.md
if(BUILD_TESTS)
	list(APPEND VCPKG_MANIFEST_FEATURES "tests")
endif()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_SOURCE_DIR}/external/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(vectrexy
	DESCRIPTION "A Vectrexy emulator by Antonio Maiorano"
	HOMEPAGE_URL https://github.com/amaiorano/vectrexy
)

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

if(UNIX AND NOT APPLE)
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

# Set C++ standard and disable extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(USE_SDL_ENGINE)
	add_compile_definitions(PLATFORM_SDL)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	add_compile_definitions(
		_CRT_SECURE_NO_WARNINGS
		_SCL_SECURE_NO_WARNINGS
	)
	add_compile_options(/MP /W4 /WX)
	# Use dynamic CRT for shared lib builds to avoid issues with allocations in one DLL
	# being freed in another. For static buids, use static CRT so the user doesn't have to
	# install the right CRT.
	if (BUILD_SHARED_LIBS)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if("x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC") # clang-cl
		add_compile_definitions(
			_CRT_SECURE_NO_WARNINGS
			_SCL_SECURE_NO_WARNINGS
		)
	endif()

	add_compile_options(
		-Wno-deprecated-declarations
		-Wno-format-security
		-Wno-nonportable-include-path
		-Wno-pragma-pack
		-Wno-unused-function
		-Wno-unused-const-variable
	)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(
		-Wno-format-security
	)
endif()
	
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
add_subdirectory(external/cppdap)

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

if(BUILD_TESTS)
	find_package(GTest CONFIG REQUIRED)
	add_subdirectory(external/subprocess)
	add_subdirectory(tests/debugger_tests)
endif()
