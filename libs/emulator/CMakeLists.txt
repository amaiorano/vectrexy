project(emulator)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SRC_FILES})

add_library(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC "include")

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        core
)
