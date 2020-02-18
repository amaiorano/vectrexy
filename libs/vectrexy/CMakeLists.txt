set(MODULE_NAME vectrexy)

include(${PROJECT_SOURCE_DIR}/cmake/Util.cmake)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

# Enable DPI awareness on Windows 8+
if(MSVC)
	set(MANIFEST_FILE ${CMAKE_SOURCE_DIR}/cmake/dpiawarescaleing.manifest)
endif()

add_executable(${MODULE_NAME} ${SRC_FILES} ${MANIFEST_FILE})

target_link_libraries(${MODULE_NAME}
	PUBLIC
		core
		emulator
		debugger
		engine
		$<$<BOOL:${USE_NULL_ENGINE}>:null_engine>
		$<$<BOOL:${USE_SDL_ENGINE}>:sdl_engine>
)
