# Force linking against GLEW static libs, even when BUILD_SHARED_LIBS is ON
set(GLEW_USE_STATIC_LIBS ON)
find_package(GLEW CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(OpenGL MODULE REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

file (GLOB_RECURSE SDL_GL_ENGINE_SRC_FILES "include/*.*" "src/*.*")
source_group(TREE "${PROJECT_SOURCE_DIR}/libs/sdl_gl_engine" FILES ${SDL_GL_ENGINE_SRC_FILES})
add_library(sdl_gl_engine ${SDL_GL_ENGINE_SRC_FILES})
target_include_directories(sdl_gl_engine PUBLIC "include")
target_link_libraries(sdl_gl_engine
	PUBLIC
		core
		emulator
		debugger
		engine
		SDL2::SDL2main

	PRIVATE
		SDL2::SDL2-static
		imgui::imgui
		GLEW::GLEW
		glm		
		${OPENGL_gl_LIBRARY}
		${OPENGL_glu_LIBRARY}
)
