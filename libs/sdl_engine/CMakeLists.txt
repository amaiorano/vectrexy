set(MODULE_NAME sdl_engine)

include(${PROJECT_SOURCE_DIR}/cmake/Util.cmake)

if(NOT USE_SDL_ENGINE)
	message(FATAL_ERROR "USE_SDL_ENGINE must be enabled to use ${MODULE_NAME}")
endif()

# Force linking against GLEW static libs, even when BUILD_SHARED_LIBS is ON
set(GLEW_USE_STATIC_LIBS ON)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL MODULE REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_net CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC_FILES})

add_library(${MODULE_NAME} ${SRC_FILES})

target_include_directories(${MODULE_NAME} PUBLIC "include")

target_link_libraries(${MODULE_NAME}
	PUBLIC
		core
		emulator
		debugger
		engine
		SDL2::SDL2main

	PRIVATE
		SDL2::SDL2
		SDL2_net::SDL2_net-static
		imgui::imgui
		GLEW::GLEW
		glm::glm
		OpenGL::GL
		OpenGL::GLU
)
