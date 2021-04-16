set(MODULE_NAME debugger_tests)

include(${PROJECT_SOURCE_DIR}/cmake/Util.cmake)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_executable(${MODULE_NAME} ${SRC_FILES} ${MANIFEST_FILE})

target_link_libraries(${MODULE_NAME}
	PRIVATE
		debugger
		GTest::gtest
		GTest::gtest_main
		subprocess
)
