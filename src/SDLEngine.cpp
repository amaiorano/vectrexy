#include "SDLEngine.h"

#include <Windows.h>
#undef min
#undef max

#include <GL/GLU.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <chrono>
#include <iostream>

// Display window dimensions
//#define WINDOW_WIDTH 480
const int WINDOW_WIDTH = 600;
//#define WINDOW_WIDTH 768
const int WINDOW_HEIGHT = static_cast<int>(WINDOW_WIDTH * 4.0f / 3.0f);

// Vectrex screen dimensions
const int SCREEN_WIDTH = 256;
const int SCREEN_HEIGHT = 256;

namespace {
    IEngineClient* g_client = nullptr;
    SDL_Window* g_window = NULL;
    SDL_GLContext g_glContext;

    void SetOpenGLVersion() {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        // SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    }

    void InitGL() {
        glShadeModel(GL_SMOOTH);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    }

    int SetViewport(int width, int height) {
        GLfloat ratio;

        if (height == 0) {
            height = 1;
        }

        ratio = GLfloat(width) / GLfloat(height);
        glViewport(0, 0, GLsizei(width), GLsizei(height));
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        double halfWidth = SCREEN_WIDTH /*static_cast<double>(width)*/ / 2.0;
        double halfHeight = SCREEN_HEIGHT /*static_cast<double>(height)*/ / 2.0;
        gluOrtho2D(-halfWidth, halfWidth, -halfHeight, halfHeight);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        return true;
    }

    void Render() {
        glLoadIdentity();

        float halfWidth = SCREEN_WIDTH / 2.0f / 2.0f;
        float halfHeight = SCREEN_HEIGHT / 2.0f / 2.0f;

        glBegin(GL_TRIANGLES);
        glVertex3f(0.0f, halfHeight, 0.0f);
        glVertex3f(-halfWidth, -halfHeight, 0.0f);
        glVertex3f(halfWidth, -halfHeight, 0.0f);
        glEnd();
    }
} // namespace

void SDLEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool SDLEngine::Run(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL cannot init with error " << SDL_GetError() << std::endl;
        return false;
    }

    SetOpenGLVersion();

    g_window = SDL_CreateWindow("Opengl", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    if (g_window == NULL) {
        std::cout << "Cannot create window with error " << SDL_GetError() << std::endl;
        return false;
    }

    g_glContext = SDL_GL_CreateContext(g_window);
    if (g_glContext == NULL) {
        std::cout << "Cannot create OpenGL context with error " << SDL_GetError() << std::endl;
        return false;
    }

    InitGL();
    SetViewport(WINDOW_WIDTH, WINDOW_HEIGHT);

    if (!g_client->Init(argc, argv)) {
        return false;
    }

    auto lastTime = std::chrono::high_resolution_clock::now();

    bool quit = false;
    SDL_Event sdlEvent;
    while (!quit) {
        while (SDL_PollEvent(&sdlEvent) != 0) {
            if (sdlEvent.type == SDL_QUIT) {
                quit = true;
            }
        }

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Render();

        const auto currTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> diff = currTime - lastTime;
        const double deltaTime = std::min(diff.count(), 1 / 100.0);
        lastTime = currTime;

        if (!g_client->Update(deltaTime))
            quit = true;

        SDL_GL_SwapWindow(g_window);
    }

    g_client->Shutdown();

    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return true;
}
