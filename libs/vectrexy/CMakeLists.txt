file (GLOB_RECURSE VECTREXY_SRC_FILES "include/*.*" "src/*.*")
source_group(TREE "${PROJECT_SOURCE_DIR}/libs/vectrexy" FILES ${VECTREXY_SRC_FILES})

# Enable DPI awareness on Windows 8+
if (MSVC)
	set(MANIFEST_FILE ${CMAKE_SOURCE_DIR}/cmake/dpiawarescaleing.manifest)
endif()

add_executable(vectrexy ${VECTREXY_SRC_FILES} ${MANIFEST_FILE})

target_link_libraries(vectrexy PRIVATE
	core
	emulator
	debugger
	engine
	sdl_gl_engine
)
