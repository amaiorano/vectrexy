set(MODULE_NAME vectrexy)

file(GLOB_RECURSE VECTREXY_SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${VECTREXY_SRC_FILES})

# Enable DPI awareness on Windows 8+
if(MSVC)
	set(MANIFEST_FILE ${CMAKE_SOURCE_DIR}/cmake/dpiawarescaleing.manifest)
endif()

add_executable(${MODULE_NAME} ${VECTREXY_SRC_FILES} ${MANIFEST_FILE})

target_link_libraries(${MODULE_NAME}
	PUBLIC
		core
		emulator
		debugger
		engine
		$<$<BOOL:${USE_NULL_ENGINE}>:null_engine>
		$<$<BOOL:${USE_SDL_ENGINE}>:sdl_engine>
)
