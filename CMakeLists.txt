cmake_minimum_required (VERSION 3.13)
project (vectrexy)

# Set module path to local cmake folder so find_package looks there first
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

find_package(SDL2 CONFIG REQUIRED)
find_package(sdl2-net CONFIG REQUIRED)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL MODULE REQUIRED)
find_package(STB MODULE REQUIRED)

if (UNIX AND NOT APPLE)
	set(LINUX true)
endif()

if (LINUX)
	find_package(GTK2 2.4 REQUIRED gtk)
	set(EXTRA_INCLUDE_DIRS ${EXTRA_INCLUDE_DIRS} ${GTK2_INCLUDE_DIRS})
	set(EXTRA_LIBS ${EXTRA_LIBS} ${GTK2_LIBRARIES})
endif()

# Enable DPI awareness on Windows 8+
if (MSVC)
	set(MANIFEST_FILE cmake/dpiawarescaleing.manifest)
endif()

# core library
file (GLOB_RECURSE CORE_SRC_FILES "libs/core/include/*.*" "libs/core/src/*.*")
source_group(TREE "../libs/core" FILES ${CORE_SRC_FILES})

add_library(core ${CORE_SRC_FILES})

target_include_directories(core PUBLIC "libs/core/include")
target_compile_features(core PUBLIC cxx_std_17)
set_target_properties(core PROPERTIES CXX_EXTENSIONS OFF)
# TODO: Remove core dependency on SDL2
target_link_libraries(core PUBLIC SDL2::SDL2_net SDL2::SDL2main SDL2::SDL2-static)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(core PUBLIC _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
	target_compile_options(core PUBLIC /MP /W4 /WX)
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
target_link_libraries(engine PUBLIC core emulator debugger)

# vectrexy exe
# TODO: third parties as separate targets
file(GLOB THIRD_PARTY_NOC "external/noc/noc_file_dialog.h")
source_group(TREE ".." FILES ${THIRD_PARTY_NOC})

if (LINUX)
	file(GLOB THIRD_PARTY_LINENOISE "external/linenoise/linenoise.*")
	source_group(TREE ".." FILES ${THIRD_PARTY_LINENOISE})
endif()

set(SRC ${THIRD_PARTY_NOC} ${THIRD_PARTY_LINENOISE})
add_executable(vectrexy ${SRC} ${MANIFEST_FILE})

#TODO: make target-based, rather than global
include_directories(
	"external"
	${STB_INCLUDE_PATH}
	${EXTRA_INCLUDE_DIRS}
)

target_link_libraries(vectrexy PRIVATE
	core
	emulator
	debugger
	engine
	# SDL2::SDL2main
	# SDL2::SDL2-static
	GLEW::GLEW
	glm
	imgui::imgui
	${OPENGL_gl_LIBRARY}
	${OPENGL_glu_LIBRARY}
	${EXTRA_LIBS}
)
