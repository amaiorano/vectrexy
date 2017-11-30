#include "SDLEngine.h"

#include "EngineClient.h"
#include "GLRender.h"
#include "StringHelpers.h"
#include "imgui_impl/imgui_impl_sdl_gl3.h"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>

//@TODO: Move to some header
template <typename Container, typename Pred>
bool find_if(Container container, Pred pred) {
    return std::find_if(std::begin(container), std::end(container), pred) != std::end(container);
}

namespace {
    // Display window dimensions
    const int DEFAULT_WINDOW_WIDTH = 600;
    inline int WindowHeightFromWidth(int width) { return static_cast<int>(width * 4.0f / 3.0f); }

    const char* WINDOW_TITLE = "Vectrexy";

    IEngineClient* g_client = nullptr;
    SDL_Window* g_window = NULL;
    SDL_GLContext g_glContext;

    template <typename T>
    constexpr T MsToSec(T ms) {
        return static_cast<T>(ms / 1000.0);
    }

    void SetOpenGLVersion() {
        auto[major, minor] = GLRender::GetMajorMinorVersion();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }

    struct Options {
        std::optional<int> windowX;
        std::optional<int> windowY;
        std::optional<int> windowWidth;
        std::optional<float> imguiFontScale;
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
            else if (tokens[0] == "imguiFontScale")
                options.imguiFontScale = std::stof(tokens[1]);
            else {
                std::cerr << "Unknown option: " << tokens[0] << std::endl;
            }
        }
        return options;
    }

    struct Gamepad {
        struct ButtonState {
            bool down = false;
            bool pressed = false;
        };

        void OnButtonStateChange(int buttonIndex, bool down) {
            auto& state = m_buttonStates[buttonIndex];
            if (down) {
                state.pressed = !state.down;
                state.down = true;
            } else {
                state.down = false;
            }
        }

        void OnAxisStateChange(int axisIndex, int32_t value) { m_axisValue[axisIndex] = value; }

        void PostFrameUpdateStates() {
            for (auto& state : m_buttonStates) {
                state.pressed = false;
            }
        }

        const ButtonState& GetButtonState(int buttonIndex) const {
            return m_buttonStates[buttonIndex];
        }

        const int32_t& GetAxisValue(int axisValue) const { return m_axisValue[axisValue]; }

    private:
        std::array<ButtonState, SDL_CONTROLLER_BUTTON_MAX> m_buttonStates;
        std::array<int32_t, SDL_CONTROLLER_AXIS_MAX> m_axisValue = {0};
    };

    std::unordered_map<int, Gamepad> g_playerIndexToGamepad;
    std::unordered_map<int, int> g_instanceIdToPlayerIndex;

    Gamepad& GetGamepadByInstanceId(int instanceId) {
        assert(g_instanceIdToPlayerIndex.find(instanceId) != g_instanceIdToPlayerIndex.end());
        int playerIndex = g_instanceIdToPlayerIndex[instanceId];
        auto iter = g_playerIndexToGamepad.find(playerIndex);
        assert(iter != g_playerIndexToGamepad.end());
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
                g_instanceIdToPlayerIndex[instanceId] = index;
                g_playerIndexToGamepad[index] = {};
            }
        }
    }

    void RemoveController(int instanceId) {
        auto controller = SDL_GameControllerFromInstanceID(instanceId);
        SDL_GameControllerClose(controller);

        int index = g_instanceIdToPlayerIndex[instanceId];
        auto iter = g_playerIndexToGamepad.find(index);
        assert(iter != g_playerIndexToGamepad.end());
        g_playerIndexToGamepad.erase(iter);
    }

    Input UpdateInput() {
        Input input;

        auto remapDigitalToAxisValue = [](auto left, auto right) -> int8_t {
            return left ? -128 : right ? 127 : 0;
        };

        auto remapAxisValue = [](int32_t value) -> int8_t {
            return static_cast<int8_t>((value / 32767.0f) * 127);
        };

        // Prefer gamepads for both players. If one gamepad, then player 2 uses keyboard. If no
        // gamepads, player 1 uses keyboard and there's no player 2 input.

        const bool playerOneHasGamepad =
            g_playerIndexToGamepad.find(0) != g_playerIndexToGamepad.end();

        const bool playerTwoHasGamepad =
            g_playerIndexToGamepad.find(1) != g_playerIndexToGamepad.end();

        if (!(playerOneHasGamepad && playerTwoHasGamepad)) {
            uint8_t joystickIndex = playerOneHasGamepad ? 1 : 0;

            auto* state = SDL_GetKeyboardState(nullptr);
            input.SetButton(joystickIndex, 0, state[SDL_SCANCODE_A] != 0);
            input.SetButton(joystickIndex, 1, state[SDL_SCANCODE_S] != 0);
            input.SetButton(joystickIndex, 2, state[SDL_SCANCODE_D] != 0);
            input.SetButton(joystickIndex, 3, state[SDL_SCANCODE_F] != 0);

            input.SetAnalogAxisX(joystickIndex, remapDigitalToAxisValue(state[SDL_SCANCODE_LEFT],
                                                                        state[SDL_SCANCODE_RIGHT]));
            input.SetAnalogAxisY(joystickIndex, remapDigitalToAxisValue(state[SDL_SCANCODE_DOWN],
                                                                        state[SDL_SCANCODE_UP]));
        }

        for (auto& p : g_playerIndexToGamepad) {
            uint8_t joystickIndex = static_cast<uint8_t>(p.first);
            auto& gamepad = p.second;

            input.SetButton(joystickIndex, 0, gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_X).down);
            input.SetButton(joystickIndex, 1, gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_A).down);
            input.SetButton(joystickIndex, 2, gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_B).down);
            input.SetButton(joystickIndex, 3, gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_Y).down);

            if (gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_LEFT).down ||
                gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_RIGHT).down) {
                input.SetAnalogAxisX(
                    joystickIndex,
                    remapDigitalToAxisValue(
                        gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_LEFT).down,
                        gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_RIGHT).down));
            } else {
                input.SetAnalogAxisX(
                    joystickIndex, remapAxisValue(gamepad.GetAxisValue(SDL_CONTROLLER_AXIS_LEFTX)));
            }

            if (gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_DOWN).down ||
                gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_UP).down) {
                input.SetAnalogAxisY(
                    joystickIndex, remapDigitalToAxisValue(
                                       gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_DOWN).down,
                                       gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_UP).down));
            } else {
                input.SetAnalogAxisY(joystickIndex, -remapAxisValue(gamepad.GetAxisValue(
                                                        SDL_CONTROLLER_AXIS_LEFTY)));
            }
        }

        return input;
    }

    class Keyboard {
    public:
        struct KeyState {
            bool down = false;
            bool pressed = false;
        };

        void OnKeyStateChange(const SDL_KeyboardEvent& keyboardEvent) {
            auto& keyState = m_keyStates[keyboardEvent.keysym.scancode];
            if (keyboardEvent.type == SDL_KEYDOWN) {
                keyState.pressed = !keyState.down;
                keyState.down = true;
            } else {
                keyState.down = false;
            }
        }

        void PostFrameUpdateKeyStates() {
            for (auto& keyState : m_keyStates) {
                keyState.pressed = false;
            }
        }

        const KeyState& GetKeyState(SDL_Scancode scancode) { return m_keyStates[scancode]; }

        void ResetKeyState(SDL_Scancode scancode) { m_keyStates[scancode] = {}; }

    private:
        std::array<KeyState, SDL_NUM_SCANCODES> m_keyStates;
    };
    Keyboard g_keyboard;

    void UpdatePauseState(bool& pause) {
        bool togglePause = false;

        if (g_keyboard.GetKeyState(SDL_SCANCODE_P).pressed) {
            togglePause = true;
        }

        for (auto& kvp : g_playerIndexToGamepad) {
            auto& gamepad = kvp.second;
            if (gamepad.GetButtonState(SDL_CONTROLLER_BUTTON_START).pressed)
                togglePause = true;
        }

        if (togglePause)
            pause = !pause;
    }

    void UpdateTurboMode(bool& turbo) {
        turbo = false;

        if (g_keyboard.GetKeyState(SDL_SCANCODE_GRAVE).down) {
            turbo = true;
        }

        for (auto& kvp : g_playerIndexToGamepad) {
            auto& gamepad = kvp.second;
            if (gamepad.GetAxisValue(SDL_CONTROLLER_AXIS_RIGHTX) > 16000)
                turbo = true;
        }
    }

} // namespace

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
                                SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (g_window == NULL) {
        std::cout << "Cannot create window with error " << SDL_GetError() << std::endl;
        return false;
    }

    g_glContext = SDL_GL_CreateContext(g_window);
    if (g_glContext == NULL) {
        std::cout << "Cannot create OpenGL context with error " << SDL_GetError() << std::endl;
        return false;
    }

    ImGui_ImplSdlGL3_Init(g_window);
    ImGui::GetIO().FontGlobalScale = options.imguiFontScale.value_or(1.f);

    GLRender::Initialize();
    GLRender::OnWindowResized(windowWidth, windowHeight);

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
            ImGui_ImplSdlGL3_ProcessEvent(&sdlEvent);
            if (sdlEvent.type == SDL_QUIT) {
                quit = true;
            }

            switch (sdlEvent.type) {
            case SDL_WINDOWEVENT:
                switch (sdlEvent.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    GLRender::OnWindowResized(sdlEvent.window.data1, sdlEvent.window.data2);
                    break;
                }
                break;

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
                GetGamepadByInstanceId(sdlEvent.cbutton.which)
                    .OnButtonStateChange(cbutton.button, buttonDown);
            } break;

            case SDL_CONTROLLERAXISMOTION: {
                const auto& caxis = sdlEvent.caxis;
                GetGamepadByInstanceId(sdlEvent.cdevice.which)
                    .OnAxisStateChange(caxis.axis, caxis.value);
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                g_keyboard.OnKeyStateChange(sdlEvent.key);
                break;
            }
        }

        // Update time
        const auto currTime = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<double> diff = currTime - lastTime;
        const double realFrameTime = diff.count();
        lastTime = currTime;

        // FPS
        {
            static double frames = 0;
            static double elapsedTime = 0;
            frames += 1;
            elapsedTime += realFrameTime;
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

        // Compute frame time
        const double frameTime = [&] {
            // Clamp
            double frameTime = std::min(realFrameTime, MsToSec(100.0));

            // Reset to 0 if paused
            static bool pause = false;
            UpdatePauseState(pause);
            if (pause)
                frameTime = 0.0;

            static bool turbo = false;
            UpdateTurboMode(turbo);
            if (turbo)
                frameTime *= 10;

            return frameTime;
        }();

        ImGui_ImplSdlGL3_NewFrame(g_window);

        auto input = UpdateInput();

        auto emuEvents = EmuEvents{};
        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_C).down) {
            emuEvents.push_back({EmuEvent::Type::BreakIntoDebugger});

            // @HACK: Because the SDL window ends up losing focus to the console window, we don't
            // get the KEY_UP events for these keys right away, so "continuing" from the console
            // ends up breaking back into it again. We avoid this by explicitly clearing the state
            // of these keys.
            g_keyboard.ResetKeyState(SDL_SCANCODE_LCTRL);
            g_keyboard.ResetKeyState(SDL_SCANCODE_C);
        }

        if (!g_client->Update(frameTime, input, emuEvents))
            quit = true;

        g_client->Render(frameTime, display);
        GLRender::RenderScene(frameTime);
        ImGui::Render();

        SDL_GL_SwapWindow(g_window);

        g_keyboard.PostFrameUpdateKeyStates();
        for (auto& kvp : g_playerIndexToGamepad) {
            kvp.second.PostFrameUpdateStates();
        }
    }

    g_client->Shutdown();

    SDL_DestroyWindow(g_window);
    SDL_Quit();

    return true;
}
