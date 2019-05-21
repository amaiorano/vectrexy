if (UNIX AND NOT APPLE)
	set(LINUX true)
endif()

file (GLOB_RECURSE CORE_SRC_FILES "include/*.*" "src/*.*")
source_group(TREE "${PROJECT_SOURCE_DIR}/libs/core" FILES ${CORE_SRC_FILES})

file(GLOB THIRD_PARTY_NOC "external/noc/noc_file_dialog.h")
source_group(TREE "${PROJECT_SOURCE_DIR}/libs/core" FILES ${THIRD_PARTY_NOC})

if (LINUX)
	file(GLOB THIRD_PARTY_LINENOISE "external/linenoise/linenoise.*")
	source_group(TREE "${PROJECT_SOURCE_DIR}/libs/core" FILES ${THIRD_PARTY_LINENOISE})
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
	PUBLIC "include"
	PRIVATE ${STB_INCLUDE_PATH} "external"
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
