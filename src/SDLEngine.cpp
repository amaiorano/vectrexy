#include "SDLEngine.h"

#include "ConsoleOutput.h"
#include "EngineClient.h"
#include "FileSystem.h"
#include "GLRender.h"
#include "GLUtil.h"
#include "Gui.h"
#include "Options.h"
#include "Platform.h"
#include "StringHelpers.h"
#include "imgui_impl/imgui_impl_sdl_gl3.h"
#include <SDL.h>
#include <SDL_net.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_map>

// Include SDL_syswm.h for SDL_GetWindowWMInfo
// This includes windows.h on Windows platforms, we have to do the usual dance of disabling certain
// warnings and undef'ing certain macros
// @TODO: #include "PlatformHeaders.h" that abstracts this?
MSC_PUSH_WARNING_DISABLE(4121)
#include <SDL_syswm.h>
#undef min
#undef max
MSC_POP_WARNING_DISABLE()

namespace {
    // Display window dimensions
    const int DEFAULT_WINDOW_WIDTH = 600;
    inline int WindowHeightFromWidth(int width) { return static_cast<int>(width * 4.0f / 3.0f); }
    inline int WindowWidthFromHeight(int height) { return static_cast<int>(height * 3.0f / 4.0f); }

    const char* WINDOW_TITLE = "Vectrexy";

    IEngineClient* g_client = nullptr;
    SDL_Window* g_window = NULL;
    SDL_GLContext g_glContext;
    Options g_options;
    namespace PauseSource {
        enum Type { Game, Menu, Size };
    }
    bool g_paused[PauseSource::Size]{};
    double g_fps{};

    template <typename T>
    constexpr T MsToSec(T ms) {
        return static_cast<T>(ms / 1000.0);
    }

    bool FindAndSetRootPath(fs::path exePath) {
        // Look for bios file in current directory and up parent dirs
        // and set current working directory to the one found.
        const char* biosRomFile = "bios_rom.bin";

        auto currDir = exePath.remove_filename();

        do {
            auto path = currDir / biosRomFile;

            if (fs::exists(currDir / biosRomFile)) {
                fs::current_path(currDir);
                Printf("Root path set to: %s\n", fs::current_path().string().c_str());
                return true;
            }
            currDir = currDir.parent_path();
        } while (!currDir.empty());

        Errorf("Bios rom file not found: %s", biosRomFile);
        return false;
    }

    Platform::WindowHandle GetMainWindowHandle() {
        SDL_SysWMinfo info{};
        SDL_GetWindowWMInfo(g_window, &info);
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        return info.info.win.window;
#endif
    }

    SDL_GLContext CreateGLContext(SDL_Window* window, bool enableGLDebugging) {
        auto[major, minor] = GLRender::GetMajorMinorVersion();
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

        if (enableGLDebugging) {
            // Create debug context so that we can use glDebugMessageCallback
            int contextFlags = 0;
            SDL_GL_GetAttribute(SDL_GL_CONTEXT_FLAGS, &contextFlags);
            contextFlags |= SDL_GL_CONTEXT_DEBUG_FLAG;
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, contextFlags);
        }

        return SDL_GL_CreateContext(window);
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
            Printf("Cannot support more than 2 gamepads\n");
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

    void UpdatePauseState(bool& paused) {
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
            paused = !paused;
    }

    bool IsPaused() {
        for (auto& v : g_paused) {
            if (v)
                return true;
        }
        return false;
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

    void HACK_Simulate3dImager(double frameTime, Input& input) {
        // @TODO: The 3D imager repeatedly sends button presses of joystick 2 button 4 at a given
        // frequency (apparently different depending on game). For now, I just enable it with a
        // static bool. Eventually, need to give user the option to enable the imager, etc.
        static volatile bool enabled = false;
        if (enabled) {
            const double imagerJoy1Button4PressRate = 1 / 26.0;
            static double timeLeft = imagerJoy1Button4PressRate;
            timeLeft -= frameTime;
            if (timeLeft <= 0) {
                input.SetButton(1, 3, true);
                timeLeft += imagerJoy1Button4PressRate;
            }
        }
    }

    void ImGui_Render() {
        GLUtil::ScopedDebugGroup sdb("ImGui");
        ImGui::Render();
    }

    bool IsWindowMaximized() { return (SDL_GetWindowFlags(g_window) & SDL_WINDOW_MAXIMIZED) != 0; }

} // namespace

// Implement EngineClient free-standing functions
void SetFocusMainWindow() {
    Platform::SetFocus(GetMainWindowHandle());
}

void SetFocusConsole() {
    Platform::SetConsoleFocus();
}

void ResetOverlay(const char* file) {
    GLRender::ResetOverlay(file);
}

void SDLEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool SDLEngine::Run(int argc, char** argv) {
    if (!FindAndSetRootPath(fs::path(fs::absolute(argv[0]))))
        return false;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cout << "SDL cannot init with error " << SDL_GetError() << std::endl;
        return false;
    }

    if (SDLNet_Init() < 0) {
        std::cout << "SDLNet failed to init with error " << SDL_GetError() << std::endl;
        return false;
    }

    g_options.Add<int>("windowX", -1);
    g_options.Add<int>("windowY", -1);
    g_options.Add<int>("windowWidth", -1);
    g_options.Add<int>("windowHeight", -1);
    g_options.Add<bool>("windowMaximized", false);
    g_options.Add<bool>("imguiDebugWindow", false);
    g_options.Add<bool>("enableGLDebugging", false);
    g_options.Add<float>("imguiFontScale", 1.0f);
    g_options.Add<std::string>("lastOpenedFile", {});
    g_options.SetFilePath(fs::absolute("options.txt"));
    g_options.Load();

    int windowX = g_options.Get<int>("windowX");
    if (windowX == -1)
        windowX = SDL_WINDOWPOS_CENTERED;

    int windowY = g_options.Get<int>("windowY");
    if (windowY == -1)
        windowY = SDL_WINDOWPOS_CENTERED;

    int windowWidth = g_options.Get<int>("windowWidth");
    int windowHeight = g_options.Get<int>("windowHeight");
    // If one dimension isn't set, we reset both to percentage of screen 0's resolution
    if (windowWidth == -1 || windowHeight == -1) {
        SDL_DisplayMode dispMode;
        if (SDL_GetCurrentDisplayMode(0, &dispMode) == 0) {
            if (dispMode.w > dispMode.h) {
                windowHeight = static_cast<int>(dispMode.h * 0.9f);
                windowWidth = WindowWidthFromHeight(windowHeight);
            } else {
                windowWidth = static_cast<int>(dispMode.w * 0.9f);
                windowHeight = WindowHeightFromWidth(windowWidth);
            }
        } else {
            windowWidth = DEFAULT_WINDOW_WIDTH;
            windowHeight = WindowHeightFromWidth(windowWidth);
        }
    }

    Uint32 windowCreateFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    if (g_options.Get<bool>("windowMaximized"))
        windowCreateFlags |= SDL_WINDOW_MAXIMIZED;

    g_window = SDL_CreateWindow(WINDOW_TITLE, windowX, windowY, windowWidth, windowHeight,
                                windowCreateFlags);
    if (g_window == NULL) {
        std::cout << "Cannot create window with error " << SDL_GetError() << std::endl;
        return false;
    }

    const bool enableGLDebugging = g_options.Get<bool>("enableGLDebugging");

    g_glContext = CreateGLContext(g_window, enableGLDebugging);
    if (g_glContext == NULL) {
        std::cout << "Cannot create OpenGL context with error " << SDL_GetError() << std::endl;
        return false;
    }

    enum SwapInterval : int { NoVSync = 0, VSync = 1, AdaptiveVSync = -1 };
    SDL_GL_SetSwapInterval(SwapInterval::NoVSync);

    Gui::EnabledWindows[Gui::Window::Debug] = g_options.Get<bool>("imguiDebugWindow");
    ImGui_ImplSdlGL3_Init(g_window);
    ImGui::GetIO().FontGlobalScale = g_options.Get<float>("imguiFontScale");

    GLRender::Initialize(enableGLDebugging);
    GLRender::OnWindowResized(windowWidth, windowHeight);

    if (!g_client->Init(argc, argv)) {
        return false;
    }

    RenderContext renderContext{};

    bool quit = false;
    while (!quit) {
        PollEvents(quit);
        UpdatePauseState(g_paused[PauseSource::Game]);
        auto input = UpdateInput();
        const auto frameTime = UpdateFrameTime();

        auto emuEvents = EmuEvents{};
        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_C).down) {
            emuEvents.push_back({EmuEvent::Type::BreakIntoDebugger});

            // @HACK: Because the SDL window ends up losing focus to the console window, we
            // don't get the KEY_UP events for these keys right away, so "continuing" from the
            // console ends up breaking back into it again. We avoid this by explicitly clearing
            // the state of these keys.
            g_keyboard.ResetKeyState(SDL_SCANCODE_LCTRL);
            g_keyboard.ResetKeyState(SDL_SCANCODE_C);
        }

        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_R).pressed) {
            emuEvents.push_back({EmuEvent::Type::Reset});
        }

        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_O).pressed) {
            emuEvents.push_back({EmuEvent::Type::OpenRomFile});
        }

        ImGui_ImplSdlGL3_NewFrame(g_window);

        // ImGui menu bar
        if (ImGui::BeginMainMenuBar()) {
            g_paused[PauseSource::Menu] = false;
            bool openAboutDialog = false;

            if (ImGui::BeginMenu("File")) {
                g_paused[PauseSource::Menu] = true;

                if (ImGui::MenuItem("Open rom...", "Ctrl+O"))
                    emuEvents.push_back({EmuEvent::Type::OpenRomFile});

                if (ImGui::MenuItem("Exit"))
                    quit = true;

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulation")) {
                g_paused[PauseSource::Menu] = true;

                if (ImGui::MenuItem("Reset", "Ctrl+R"))
                    emuEvents.push_back({EmuEvent::Type::Reset});

                ImGui::MenuItem("Pause", "P", &g_paused[PauseSource::Game]);

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug")) {
                g_paused[PauseSource::Menu] = true;

                ImGui::MenuItem("Debug window", "", &Gui::EnabledWindows[Gui::Window::Debug]);

                if (ImGui::MenuItem("Break into Debugger", "Ctrl+C"))
                    emuEvents.push_back({EmuEvent::Type::BreakIntoDebugger});

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                g_paused[PauseSource::Menu] = true;

                if (ImGui::MenuItem("About Vectrexy"))
                    openAboutDialog = true;
                ImGui::EndMenu();
            }

            auto RightAlignLabelText = [](const char* text) {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(text).x);
                ImGui::LabelText("", text);
            };
            RightAlignLabelText(
                FormattedString<>("%.2f FPS (%.2f ms)", g_fps, g_fps > 0 ? (1000.f / g_fps) : 0.f));

            ImGui::EndMainMenuBar();

            // Handle popups

            if (openAboutDialog) {
                ImGui::OpenPopup("About Vectrexy");
            }
            if (bool open = true; ImGui::BeginPopupModal("About Vectrexy", &open,
                                                         ImGuiWindowFlags_AlwaysAutoResize)) {
                g_paused[PauseSource::Menu] = true;

                ImGui::Text("Vectrexy");
                ImGui::Text("Programmed by Antonio Maiorano (amaiorano@gmail.com)");

                ImGui::Text("Available at");
                ImGui::SameLine();
                if (ImGui::SmallButton("github.com/amaiorano/vectrexy")) {
                    Platform::ExecuteShellCommand("https://github.com/amaiorano/vectrexy");
                }

                ImGui::Text("See");
                ImGui::SameLine();
                if (ImGui::SmallButton("README")) {
                    Platform::ExecuteShellCommand("README.md");
                }
                ImGui::SameLine();
                ImGui::Text("for more details");

                ImGui::EndPopup();
            }
        }

        //@TODO: save imguiEnabledWindows#Name and just iterate here
        static auto lastEnabledWindows = Gui::EnabledWindows;
        if (Gui::EnabledWindows[Gui::Window::Debug] != lastEnabledWindows[Gui::Window::Debug]) {
            g_options.Set("imguiDebugWindow", Gui::EnabledWindows[Gui::Window::Debug]);
            g_options.Save();
        }
        lastEnabledWindows = Gui::EnabledWindows;

        HACK_Simulate3dImager(frameTime, input);

        if (!g_client->FrameUpdate(frameTime, input, {std::ref(emuEvents), std::ref(g_options)},
                                   renderContext))
            quit = true;

        GLRender::RenderScene(frameTime, renderContext);
        ImGui_Render();
        SDL_GL_SwapWindow(g_window);

        // Don't clear lines when paused
        if (frameTime > 0) {
            renderContext.lines.clear();
        }

        g_keyboard.PostFrameUpdateKeyStates();
        for (auto& kvp : g_playerIndexToGamepad) {
            kvp.second.PostFrameUpdateStates();
        }
    }

    g_client->Shutdown();

    SDL_DestroyWindow(g_window);
    SDLNet_Quit();
    SDL_Quit();

    return true;
}

void SDLEngine::PollEvents(bool& quit) {
    SDL_Event sdlEvent;

    while (SDL_PollEvent(&sdlEvent) != 0) {
        ImGui_ImplSdlGL3_ProcessEvent(&sdlEvent);
        if (sdlEvent.type == SDL_QUIT) {
            quit = true;
        }

        switch (sdlEvent.type) {
        case SDL_WINDOWEVENT:
            switch (sdlEvent.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED: {
                const int width = sdlEvent.window.data1;
                const int height = sdlEvent.window.data2;
                if (!IsWindowMaximized()) {
                    g_options.Set("windowWidth", width);
                    g_options.Set("windowHeight", height);
                }
                g_options.Set("windowMaximized", IsWindowMaximized());
                g_options.Save();
                GLRender::OnWindowResized(width, height);
            } break;

            case SDL_WINDOWEVENT_MOVED: {
                const int x = sdlEvent.window.data1;
                const int y = sdlEvent.window.data2;
                if (!IsWindowMaximized()) {
                    g_options.Set("windowX", x);
                    g_options.Set("windowY", y);
                }
                g_options.Set("windowMaximized", IsWindowMaximized());
                g_options.Save();
            }
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
}

double SDLEngine::UpdateFrameTime() {
    static auto lastTime = std::chrono::high_resolution_clock::now();
    const auto currTime = std::chrono::high_resolution_clock::now();
    const std::chrono::duration<double> diff = currTime - lastTime;
    const double realFrameTime = diff.count();
    lastTime = currTime;

    // FPS
    static double frames = 0;
    static double elapsedTime = 0;
    frames += 1;
    elapsedTime += realFrameTime;
    if (elapsedTime >= 1) {
        g_fps = frames / elapsedTime;
        frames = 0;
        elapsedTime = elapsedTime - 1.0;
    }

    // Clamp
    double frameTime = std::min(realFrameTime, MsToSec(100.0));

    // Reset to 0 if paused
    if (IsPaused())
        frameTime = 0.0;

    // Scale up if turbo
    static bool turbo = false;
    UpdateTurboMode(turbo);
    if (turbo)
        frameTime *= 10;

    return frameTime;
}
