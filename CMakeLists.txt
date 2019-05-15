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

add_subdirectory(libs/core)
add_subdirectory(libs/engine)
add_subdirectory(libs/emulator)
add_subdirectory(libs/debugger)
add_subdirectory(libs/sdl_gl_engine)
add_subdirectory(libs/vectrexy)
