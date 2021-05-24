set(MODULE_NAME stabs_parser_tests)

file(GLOB_RECURSE SRC_FILES "*.cpp" "*.h")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_executable(${MODULE_NAME} ${SRC_FILES})

target_link_libraries(${MODULE_NAME}
	PRIVATE
		stabs_parser
		GTest::gtest
		GTest::gtest_main
)
