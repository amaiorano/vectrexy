project(core)

if (UNIX AND NOT APPLE)
	set(LINUX true)
endif()

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SRC_FILES})

file(GLOB NOC_SRC_FILES "external/noc/noc_file_dialog.h")
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${NOC_SRC_FILES})

if (LINUX)
	file(GLOB LINENOISE_SRC_FILES "external/linenoise/linenoise.*")
	source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${LINENOISE_SRC_FILES})
endif()

add_library(${PROJECT_NAME} ${SRC_FILES} ${NOC_SRC_FILES} ${LINENOISE_SRC_FILES})

find_package(SDL2 CONFIG REQUIRED)
find_package(sdl2-net CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(STB MODULE REQUIRED)
if (LINUX)
	find_package(GTK2 2.4 REQUIRED)
	target_include_directories(${PROJECT_NAME} PRIVATE ${GTK2_INCLUDE_DIRS})
	target_link_libraries(${PROJECT_NAME} PRIVATE ${GTK2_LIBRARIES})
endif()

# Make a wrapper module for STB
add_library(STB INTERFACE)
target_include_directories(STB INTERFACE ${STB_INCLUDE_PATH})

target_include_directories(${PROJECT_NAME}
	PUBLIC "include"
	PRIVATE "external"
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		SDL2::SDL2-static
		SDL2::SDL2_net
		imgui::imgui
	PRIVATE
		STB
)

# TODO: Move this out of Core and make it something each module can include
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(${PROJECT_NAME} PUBLIC _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS)
	target_compile_options(${PROJECT_NAME} PUBLIC /MP /W4 /WX)
		# By default, CMake uses dynamic CRT, which is good if we're building DLLs to avoid
		# issues with allocations in one DLL trying to be freed in another. But when building
		# statically, force using the static CRT so we don't have to worry about user having
		# the right CRT installed.
		if (NOT BUILD_SHARED_LIBS)
			target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Debug>:/MTd>")
			target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:Release>:/MT>")
			target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:MinSizeRel>:/MT>")
			target_compile_options(${PROJECT_NAME} PUBLIC "$<$<CONFIG:RelWithDebInfo>:/MT>")
		endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	# ...
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME} PUBLIC -Wno-format-security) # todo: remove this and fix warnings
	target_link_libraries(${PROJECT_NAME} PUBLIC stdc++fs) # required for filesystem
endif()
