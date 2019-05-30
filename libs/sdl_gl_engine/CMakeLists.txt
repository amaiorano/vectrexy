project(sdl_gl_engine)

if(NOT USE_SDL_ENGINE)
	message(FATAL_ERROR "USE_SDL_ENGINE must be enabled to use ${PROJECT_NAME}")
endif()

# Force linking against GLEW static libs, even when BUILD_SHARED_LIBS is ON
set(GLEW_USE_STATIC_LIBS ON)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL MODULE REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(sdl2-net CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

file(GLOB_RECURSE SRC_FILES "include/*.*" "src/*.*")
source_group(TREE ${PROJECT_SOURCE_DIR} FILES ${SRC_FILES})

add_library(${PROJECT_NAME} ${SRC_FILES})

target_include_directories(${PROJECT_NAME} PUBLIC "include")

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		core
		emulator
		debugger
		engine
		SDL2::SDL2main

	PRIVATE
		SDL2::SDL2-static
		SDL2::SDL2_net
		imgui::imgui
		GLEW::GLEW
		glm
		OpenGL::GL
		OpenGL::GLU
)
