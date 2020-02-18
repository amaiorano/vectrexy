set(MODULE_NAME emulator)

include(${PROJECT_SOURCE_DIR}/cmake/Util.cmake)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_library(${MODULE_NAME} ${SRC_FILES})

target_include_directories(${MODULE_NAME} PUBLIC "include")

target_link_libraries(${MODULE_NAME}
    PUBLIC
        core
)
