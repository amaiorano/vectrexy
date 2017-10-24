#include "SDLEngine.h"

#include "EngineClient.h"
#include <SDL.h>
#include <SDL_opengl.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>

#include <GL/GLU.h>

#include "Base.h" //@TODO: remove this?
#include "StringHelpers.h"

namespace {
    // Display window dimensions
    const int DEFAULT_WINDOW_WIDTH = 600;
    inline int WindowHeightFromWidth(int width) { return static_cast<int>(width * 4.0f / 3.0f); }

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
        std::optional<int> windowWidth;
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
            line = Trim(line);
            if (line[0] == ';') // Skip comments
                continue;

            auto tokens = Trim(Split(line, "="));
            if (tokens.size() < 2)
                continue;
            if (tokens[0] == "windowX")
                options.windowX = std::stoi(tokens[1]);
            else if (tokens[0] == "windowY")
                options.windowY = std::stoi(tokens[1]);
            else if (tokens[0] == "windowWidth")
                options.windowWidth = std::stoi(tokens[1]);
            else {
                std::cerr << "Unknown option: " << tokens[0] << std::endl;
            }
        }
        return options;
    }

    struct GamepadState {
        std::array<bool, SDL_CONTROLLER_BUTTON_MAX> buttonDown = {false};
        std::array<int32_t, SDL_CONTROLLER_AXIS_MAX> axisValue = {0};
    };
    std::unordered_map<int, GamepadState> m_gamepadStatesByPlayerIndex;

    std::unordered_map<int, int> g_controllerIndexToInstanceId;

    GamepadState& GetGamepadStateByInstanceId(int instanceId) {
        assert(g_controllerIndexToInstanceId.find(instanceId) !=
               g_controllerIndexToInstanceId.end());
        int index = g_controllerIndexToInstanceId[instanceId];

        auto iter = m_gamepadStatesByPlayerIndex.find(index);
        assert(iter != m_gamepadStatesByPlayerIndex.end());
        return iter->second;
    }

    void AddController(int index) {
        if (index >= 2) {
            printf("Cannot support more than 2 gamepads\n");
            return;
        }

        if (SDL_IsGameController(index)) {
            SDL_GameController* controller = SDL_GameControllerOpen(index);
            if (controller) {
                auto joy = SDL_GameControllerGetJoystick(controller);
                auto instanceId = SDL_JoystickInstanceID(joy);
                g_controllerIndexToInstanceId[index] = instanceId;
                m_gamepadStatesByPlayerIndex.insert({index, {}});
            }
        }
    }

    void RemoveController(int instanceId) {
        auto controller = SDL_GameControllerFromInstanceID(instanceId);
        SDL_GameControllerClose(controller);

        int index = g_controllerIndexToInstanceId[instanceId];
        auto iter = m_gamepadStatesByPlayerIndex.find(index);
        assert(iter != m_gamepadStatesByPlayerIndex.end());
        m_gamepadStatesByPlayerIndex.erase(iter);
    }

    Input UpdateInput() {
        Input input;

        auto remapDigitalToAxisValue = [](auto left, auto right) -> int8_t {
            return left ? -128 : right ? 127 : 0;
        };

        auto remapAxisValue = [](int32_t value) -> int8_t {
            return static_cast<int8_t>((value / 32767.0f) * 127);
        };

        const bool playerOneHasGamepad =
            m_gamepadStatesByPlayerIndex.find(0) != m_gamepadStatesByPlayerIndex.end();

        if (!playerOneHasGamepad) {

            auto* state = SDL_GetKeyboardState(nullptr);
            input.SetButton(0, 0, state[SDL_SCANCODE_A] != 0);
            input.SetButton(0, 1, state[SDL_SCANCODE_S] != 0);
            input.SetButton(0, 2, state[SDL_SCANCODE_D] != 0);
            input.SetButton(0, 3, state[SDL_SCANCODE_F] != 0);

            input.SetAnalogAxisX(
                0, remapDigitalToAxisValue(state[SDL_SCANCODE_LEFT], state[SDL_SCANCODE_RIGHT]));
            input.SetAnalogAxisY(
                0, remapDigitalToAxisValue(state[SDL_SCANCODE_DOWN], state[SDL_SCANCODE_UP]));

        } else {

            for (auto& p : m_gamepadStatesByPlayerIndex) {
                uint8_t joystickIndex = static_cast<uint8_t>(p.first);
                auto& gamepadState = p.second;
                auto& buttonState = gamepadState.buttonDown;

                input.SetButton(joystickIndex, 0, buttonState[SDL_CONTROLLER_BUTTON_X]);
                input.SetButton(joystickIndex, 1, buttonState[SDL_CONTROLLER_BUTTON_A]);
                input.SetButton(joystickIndex, 2, buttonState[SDL_CONTROLLER_BUTTON_B]);
                input.SetButton(joystickIndex, 3, buttonState[SDL_CONTROLLER_BUTTON_Y]);

                if (buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT] ||
                    buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]) {
                    input.SetAnalogAxisX(
                        joystickIndex,
                        remapDigitalToAxisValue(buttonState[SDL_CONTROLLER_BUTTON_DPAD_LEFT],
                                                buttonState[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]));
                } else {
                    input.SetAnalogAxisX(
                        joystickIndex,
                        remapAxisValue(gamepadState.axisValue[SDL_CONTROLLER_AXIS_LEFTX]));
                }

                if (buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN] ||
                    buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP]) {
                    input.SetAnalogAxisY(
                        joystickIndex,
                        remapDigitalToAxisValue(buttonState[SDL_CONTROLLER_BUTTON_DPAD_DOWN],
                                                buttonState[SDL_CONTROLLER_BUTTON_DPAD_UP]));
                } else {
                    input.SetAnalogAxisY(
                        joystickIndex,
                        -remapAxisValue(gamepadState.axisValue[SDL_CONTROLLER_AXIS_LEFTY]));
                }
            }
        }

        return input;
    }

} // namespace

void Display::Clear() {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Display::DrawLine(float x1, float y1, float x2, float y2) {
    auto AlmostEqual = [](float a, float b, float epsilon = 0.01f) {
        return abs(a - b) <= epsilon;
    };

    if (AlmostEqual(x1, x2) && AlmostEqual(y1, y2)) {
        glBegin(GL_POINTS);
        glVertex2f(x1, y1);
        glEnd();
    } else {
        glBegin(GL_LINES);
        glVertex2f(x1, y1);
        glVertex2f(x2, y2);
        glEnd();
    }
}

void SDLEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool SDLEngine::Run(int argc, char** argv) {
    const auto options = LoadOptionsFile("options.txt");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cout << "SDL cannot init with error " << SDL_GetError() << std::endl;
        return false;
    }

    SetOpenGLVersion();

    const int windowX = options.windowX.value_or(SDL_WINDOWPOS_CENTERED);
    const int windowY = options.windowY.value_or(SDL_WINDOWPOS_CENTERED);
    const int windowWidth = options.windowWidth.value_or(DEFAULT_WINDOW_WIDTH);
    const int windowHeight = WindowHeightFromWidth(windowWidth);

    g_window = SDL_CreateWindow(WINDOW_TITLE, windowX, windowY, windowWidth, windowHeight,
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
    SetViewport(windowWidth, windowHeight);

    if (!g_client->Init(argc, argv)) {
        return false;
    }

    Display display;
    // Clear at least once, required for certain drivers that don't clear the buffers
    display.Clear();

    auto lastTime = std::chrono::high_resolution_clock::now();

    bool quit = false;
    SDL_Event sdlEvent;
    while (!quit) {
        while (SDL_PollEvent(&sdlEvent) != 0) {
            if (sdlEvent.type == SDL_QUIT) {
                quit = true;
            }

            switch (sdlEvent.type) {
            case SDL_CONTROLLERDEVICEADDED:
                AddController(sdlEvent.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                RemoveController(sdlEvent.cdevice.which);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                const auto& cbutton = sdlEvent.cbutton;
                const bool buttonDown = sdlEvent.type == SDL_CONTROLLERBUTTONDOWN;
                GetGamepadStateByInstanceId(sdlEvent.cbutton.which).buttonDown[cbutton.button] =
                    buttonDown;
            } break;

            case SDL_CONTROLLERAXISMOTION: {
                const auto& caxis = sdlEvent.caxis;
                GetGamepadStateByInstanceId(sdlEvent.cdevice.which).axisValue[caxis.axis] =
                    caxis.value;
            } break;
            }
        }

        const auto currTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> diff = currTime - lastTime;
        const double deltaTime = std::min(diff.count(), 1 / 100.0);
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

        auto input = UpdateInput();

        if (!g_client->Update(deltaTime, input))
            quit = true;

        glLoadIdentity();
        g_client->Render(deltaTime, display);

        SDL_GL_SwapWindow(g_window);
    }

    g_client->Shutdown();

    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return true;
}
