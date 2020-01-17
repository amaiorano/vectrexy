set(MODULE_NAME debugger)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

#message(${SRC_FILES})
set_source_files_properties(
    C:/code/vectrexy/libs/debugger/src/GdbStub.cpp
    PROPERTIES COMPILE_FLAGS -Od
)

add_library(${MODULE_NAME} ${SRC_FILES})

target_include_directories(${MODULE_NAME} PUBLIC "include")

target_link_libraries(${MODULE_NAME}
    PUBLIC
        core
        emulator
)
