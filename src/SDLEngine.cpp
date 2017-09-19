#include "SDLEngine.h"

#include <Windows.h>
#undef min
#undef max

#include <GL/GLU.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>

#include "Base.h" //@TODO: remove this?
#include "StringHelpers.h"

namespace {
    // Display window dimensions
    //#define WINDOW_WIDTH 480
    const int WINDOW_WIDTH = 600;
    //#define WINDOW_WIDTH 768
    const int WINDOW_HEIGHT = static_cast<int>(WINDOW_WIDTH * 4.0f / 3.0f);

    // Vectrex screen dimensions
    const int SCREEN_WIDTH = 256;
    const int SCREEN_HEIGHT = 256;

    const char* WINDOW_TITLE = "Vectrexy";

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

    struct Options {
        std::optional<int> windowX;
        std::optional<int> windowY;
    };

    Options LoadOptionsFile(const char* file) {
        Options options;

        std::ifstream fin(file);
        if (!fin) {
            std::cerr << "No options file \"" << file << "\" found, using default values"
                      << std::endl;
            return options;
        }

        std::string line;
        while (std::getline(fin, line)) {
            auto tokens = Trim(Split(line, "="));

            if (tokens.size() < 2)
                continue;
            if (tokens[0] == "windowX")
                options.windowX = std::stoi(tokens[1]);
            else if (tokens[0] == "windowY")
                options.windowY = std::stoi(tokens[1]);
            else {
                std::cerr << "Unknown option: " << tokens[0] << std::endl;
            }
        }
        return options;
    }

} // namespace

void Display::Clear() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Display::DrawLine(float x1, float y1, float x2, float y2) {
    glBegin(GL_LINES);
    glVertex2f(x1, y1);
    glVertex2f(x2, y2);
    glEnd();
}

void SDLEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool SDLEngine::Run(int argc, char** argv) {
    const auto options = LoadOptionsFile("options.txt");

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "SDL cannot init with error " << SDL_GetError() << std::endl;
        return false;
    }

    SetOpenGLVersion();

    int windowX = options.windowX ? *options.windowX : SDL_WINDOWPOS_CENTERED;
    int windowY = options.windowY ? *options.windowY : SDL_WINDOWPOS_CENTERED;
    g_window = SDL_CreateWindow(WINDOW_TITLE, windowX, windowY, WINDOW_WIDTH, WINDOW_HEIGHT,
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
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

    Display display;
    auto lastTime = std::chrono::high_resolution_clock::now();

    bool quit = false;
    SDL_Event sdlEvent;
    while (!quit) {
        while (SDL_PollEvent(&sdlEvent) != 0) {
            if (sdlEvent.type == SDL_QUIT) {
                quit = true;
            }
        }

        glLoadIdentity();
        g_client->Render(display);

        const auto currTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> diff = currTime - lastTime;
        const double deltaTime = std::min(diff.count(), 1 / 10.0);
        lastTime = currTime;

        // FPS
        {
            static double frames = 0;
            static double elapsedTime = 0;
            frames += 1;
            elapsedTime += deltaTime;
            if (elapsedTime >= 1) {
                double currFps = frames / elapsedTime;
                static double avgFps = currFps;
                avgFps = avgFps * 0.75 + currFps * 0.25;

                SDL_SetWindowTitle(g_window, FormattedString<>("%s - FPS: %.2f (avg: %.2f)",
                                                               WINDOW_TITLE, currFps, avgFps));

                frames = 0;
                elapsedTime = elapsedTime - 1.0;
            }
        }

        if (!g_client->Update(deltaTime))
            quit = true;

        SDL_GL_SwapWindow(g_window);
    }

    g_client->Shutdown();

    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return true;
}
