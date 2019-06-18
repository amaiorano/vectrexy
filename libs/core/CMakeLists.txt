set(MODULE_NAME core)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_library(${MODULE_NAME} ${SRC_FILES})

if(USE_SDL_ENGINE)
	find_package(SDL2 CONFIG REQUIRED)
	find_package(sdl2-net CONFIG REQUIRED)
endif()
if(DEBUG_UI)
	find_package(imgui CONFIG REQUIRED)
endif()
find_package(STB MODULE REQUIRED)
if(LINUX)
	find_package(GTK2 2.4 REQUIRED)
	target_include_directories(${MODULE_NAME} PRIVATE ${GTK2_INCLUDE_DIRS})
	target_link_libraries(${MODULE_NAME} PRIVATE ${GTK2_LIBRARIES})
endif()

# Make a wrapper module for STB
add_library(STB INTERFACE)
target_include_directories(STB INTERFACE ${STB_INCLUDE_PATH})

target_include_directories(${MODULE_NAME}
	PUBLIC "include"
)

target_link_libraries(${MODULE_NAME}
	PUBLIC
		$<$<BOOL:${DEBUG_UI}>:imgui::imgui>
	PRIVATE
		STB
		noc
		$<$<BOOL:${LINUX}>:linenoise>
		$<$<BOOL:${USE_SDL_ENGINE}>:SDL2::SDL2-static>
		$<$<BOOL:${USE_SDL_ENGINE}>:SDL2::SDL2_net>
)

# TODO: Move this out of Core and make it something each module can include
target_compile_features(${MODULE_NAME} PUBLIC cxx_std_17)
set_target_properties(${MODULE_NAME} PROPERTIES CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_definitions(${MODULE_NAME} PUBLIC
		_CRT_SECURE_NO_WARNINGS
		_SCL_SECURE_NO_WARNINGS
	)
	target_compile_options(${MODULE_NAME} PUBLIC /MP /W4 /WX)
		# By default, CMake uses dynamic CRT, which is good if we're building DLLs to avoid
		# issues with allocations in one DLL trying to be freed in another. But when building
		# statically, force using the static CRT so we don't have to worry about user having
		# the right CRT installed.
		if(NOT BUILD_SHARED_LIBS)
			target_compile_options(${MODULE_NAME} PUBLIC "$<$<CONFIG:Debug>:/MTd>")
			target_compile_options(${MODULE_NAME} PUBLIC "$<$<CONFIG:Release>:/MT>")
			target_compile_options(${MODULE_NAME} PUBLIC "$<$<CONFIG:MinSizeRel>:/MT>")
			target_compile_options(${MODULE_NAME} PUBLIC "$<$<CONFIG:RelWithDebInfo>:/MT>")
		endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	if("x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC") # clang-cl
		target_compile_definitions(${MODULE_NAME} PUBLIC
			_CRT_SECURE_NO_WARNINGS
			_SCL_SECURE_NO_WARNINGS
		)
	else() # clang
		target_link_libraries(${MODULE_NAME} PUBLIC
			stdc++fs # required for filesystem
		)
	endif()

	target_compile_options(${MODULE_NAME} PUBLIC
		-Wno-deprecated-declarations
		-Wno-format-security
		-Wno-nonportable-include-path
		-Wno-pragma-pack
		-Wno-unused-function
		-Wno-unused-const-variable
	)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${MODULE_NAME} PUBLIC
		-Wno-format-security
	)
	target_link_libraries(${MODULE_NAME} PUBLIC
		stdc++fs # required for filesystem
	)
endif()
