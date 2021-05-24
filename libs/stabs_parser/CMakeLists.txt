cmake_minimum_required (VERSION 3.15)

project(stabs_parser
	DESCRIPTION "A STABS parser by by Antonio Maiorano"
	#HOMEPAGE_URL https://github.com/amaiorano/stabs_parser
)

set(MODULE_NAME stabs_parser)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_library(${MODULE_NAME} ${SRC_FILES})

target_include_directories(${MODULE_NAME} PUBLIC "include")

if(NOT TARGET taocpp::pegtl)
    message(FATAL_ERROR "taocpp::pegtl target is not loaded")
endif()

if(BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    add_subdirectory(tests)
endif()

target_link_libraries(${MODULE_NAME}
    PRIVATE
        core
        taocpp::pegtl
)
