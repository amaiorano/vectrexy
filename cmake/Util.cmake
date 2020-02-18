# Utility functions for CMake

# Disable optimizations for input source files, relative to CMAKE_CURRENT_SOURCE_DIR.
# Useful for debugging files in non-debug builds.
#
# Example:
#
# add_library(${MODULE_NAME} ${SRC_FILES})
# disable_opt_single_files(src/Cpu.cpp)
#
function(disable_opt_single_files source_files)
	foreach(file ${source_files})
		set(file ${CMAKE_CURRENT_SOURCE_DIR}/${file})
		message("Disabling optimizations for ${file}")
		set_source_files_properties(${file} PROPERTIES COMPILE_FLAGS -Od)
	endforeach()
endfunction()
