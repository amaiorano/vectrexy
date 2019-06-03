file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/external/linenoise/linenoise/*.*")
source_group(TREE ${CMAKE_SOURCE_DIR}/external/linenoise FILES ${SRC_FILES})
add_library(linenoise ${SRC_FILES})
target_include_directories(linenoise PUBLIC ${CMAKE_SOURCE_DIR}/external/linenoise)
