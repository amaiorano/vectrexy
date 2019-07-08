#include "sdl_engine/SDLEngine.h"

#include "GLRender.h"
#include "GLUtil.h"
#include "SDLAudioDriver.h"
#include "SDLGameController.h"
#include "SDLKeyboard.h"
#include "core/ConsoleOutput.h"
#include "core/FileSystem.h"
#include "core/FrameTimer.h"
#include "core/Gui.h"
#include "core/Platform.h"
#include "core/StringUtil.h"
#include "engine/EngineClient.h"
#include "engine/Options.h"
#include "engine/Paths.h"
#include "imgui_impl/imgui_impl_sdl_gl3.h"
#include <SDL.h>
#include <SDL_net.h>
#include <algorithm>
#include <fstream>
#include <iostream>

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

    IEngineClient* g_client = nullptr;
    SDL_Window* g_window = nullptr;
    SDL_GLContext g_glContext;
    GLRender g_glRender;
    SDLGameControllerDriver g_controllerDriver;
    SDLKeyboard g_keyboard;
    SDLAudioDriver g_audioDriver;
    Options g_options;
    namespace PauseSource {
        enum Type { Game, Menu, Size };
    }
    bool g_paused[PauseSource::Size]{};
    bool g_turbo = false;
    FrameTimer g_frameTimer;

    bool FindAndSetRootPath(fs::path exePath) {
        // Look for bios file in current directory and up parent dirs
        // and set current working directory to the one found.
        fs::path biosRomFile = Paths::biosRomFile;

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

        Errorf("Bios rom file not found: %s", biosRomFile.c_str());
        return false;
    }

    Platform::WindowHandle GetMainWindowHandle() {
        SDL_SysWMinfo info{};
        SDL_GetWindowWMInfo(g_window, &info);
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        return info.info.win.window;
#else
        return {};
#endif
    }

    SDL_GLContext CreateGLContext(SDL_Window* window, bool enableGLDebugging) {
        auto [major, minor] = g_glRender.GetMajorMinorVersion();
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

        const bool playerOneHasGamepad = g_controllerDriver.IsControllerConnected(0);
        const bool playerTwoHasGamepad = g_controllerDriver.IsControllerConnected(1);

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

        for (int i = 0; i < g_controllerDriver.NumControllers(); ++i) {
            auto joystickIndex = static_cast<uint8_t>(i);
            auto& controller = g_controllerDriver.ControllerByIndex(joystickIndex);

            input.SetButton(joystickIndex, 0,
                            controller.GetButtonState(SDL_CONTROLLER_BUTTON_X).down);
            input.SetButton(joystickIndex, 1,
                            controller.GetButtonState(SDL_CONTROLLER_BUTTON_A).down);
            input.SetButton(joystickIndex, 2,
                            controller.GetButtonState(SDL_CONTROLLER_BUTTON_B).down);
            input.SetButton(joystickIndex, 3,
                            controller.GetButtonState(SDL_CONTROLLER_BUTTON_Y).down);

            if (controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_LEFT).down ||
                controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_RIGHT).down) {
                input.SetAnalogAxisX(
                    joystickIndex,
                    remapDigitalToAxisValue(
                        controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_LEFT).down,
                        controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_RIGHT).down));
            } else {
                input.SetAnalogAxisX(joystickIndex, remapAxisValue(controller.GetAxisValue(
                                                        SDL_CONTROLLER_AXIS_LEFTX)));
            }

            if (controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_DOWN).down ||
                controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_UP).down) {
                input.SetAnalogAxisY(
                    joystickIndex,
                    remapDigitalToAxisValue(
                        controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_DOWN).down,
                        controller.GetButtonState(SDL_CONTROLLER_BUTTON_DPAD_UP).down));
            } else {
                input.SetAnalogAxisY(joystickIndex, -remapAxisValue(controller.GetAxisValue(
                                                        SDL_CONTROLLER_AXIS_LEFTY)));
            }
        }

        return input;
    }

    void UpdatePauseState(bool& paused) {
        bool togglePause = false;

        if (g_keyboard.GetKeyState(SDL_SCANCODE_P).pressed) {
            togglePause = true;
        }

        for (int i = 0; i < g_controllerDriver.NumControllers(); ++i) {
            auto& controller = g_controllerDriver.ControllerByIndex(i);
            if (controller.GetButtonState(SDL_CONTROLLER_BUTTON_START).pressed)
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

    void UpdateTurboMode() {
        g_turbo = false;

        if (g_keyboard.GetKeyState(SDL_SCANCODE_GRAVE).down) {
            g_turbo = true;
        }

        for (int i = 0; i < g_controllerDriver.NumControllers(); ++i) {
            auto& controller = g_controllerDriver.ControllerByIndex(i);
            if (controller.GetAxisValue(SDL_CONTROLLER_AXIS_RIGHTX) > 16000)
                g_turbo = true;
        }
    }

    bool IsTurboMode() { return g_turbo; }

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

    std::optional<float> GetDPI() {
        float ddpi, hdpi, vdpi;
        if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0)
            return ddpi;
        return {};
    }

    float GetDefaultImguiFontScale() {
        if (auto dpi = GetDPI()) {
            return *dpi / 96.f;
        }
        return 1.f;
    }

} // namespace

void SDLEngine::RegisterClient(IEngineClient& client) {
    g_client = &client;
}

bool SDLEngine::Run(int argc, char** argv) {
    if (!FindAndSetRootPath(fs::path(fs::absolute(argv[0]))))
        return false;

    // Create standard directories
    fs::create_directories(Paths::overlaysDir);
    fs::create_directories(Paths::romsDir);
    fs::create_directories(Paths::userDir);
    fs::create_directories(Paths::devDir);

    std::shared_ptr<IEngineService> engineService =
        std::make_shared<aggregate_adapter<IEngineService>>(
            // SetFocusMainWindow
            [] { Platform::SetFocus(GetMainWindowHandle()); },
            // SetFocusConsole
            [] { Platform::SetConsoleFocus(); },
            // ResetOverlay
            [](const char* file) { g_glRender.ResetOverlay(file); });

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
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
    g_options.Add<float>("imguiFontScale", GetDefaultImguiFontScale());
    g_options.Add<std::string>("lastOpenedFile", {});
    g_options.SetFilePath(Paths::optionsFile);
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

    std::string windowTitle = "Vectrexy";
    if (auto fin = std::ifstream("version.txt"); fin) {
        std::string version;
        fin >> version;
        windowTitle += " " + version;
    }

    g_window = SDL_CreateWindow(windowTitle.c_str(), windowX, windowY, windowWidth, windowHeight,
                                windowCreateFlags);
    if (g_window == nullptr) {
        std::cout << "Cannot create window with error " << SDL_GetError() << std::endl;
        return false;
    }

    const bool enableGLDebugging = g_options.Get<bool>("enableGLDebugging");

    g_glContext = CreateGLContext(g_window, enableGLDebugging);
    if (g_glContext == nullptr) {
        std::cout << "Cannot create OpenGL context with error " << SDL_GetError() << std::endl;
        return false;
    }

    // TODO: Expose as option
    enum SwapInterval : int { NoVSync = 0, VSync = 1, AdaptiveVSync = -1 };
    if (SDL_GL_SetSwapInterval(SwapInterval::AdaptiveVSync) == -1) {
        SDL_GL_SetSwapInterval(SwapInterval::VSync);
    }

#ifdef DEBUG_UI_ENABLED
    Gui::EnabledWindows[Gui::Window::Debug] = g_options.Get<bool>("imguiDebugWindow");
#endif

    ImGui_ImplSdlGL3_Init(g_window);
    ImGui::GetIO().FontGlobalScale = g_options.Get<float>("imguiFontScale");
    static const auto imguiIniFilePath = Paths::imguiIniFile.string();
    ImGui::GetIO().IniFilename = imguiIniFilePath.c_str();

    g_glRender.Initialize(enableGLDebugging);
    g_glRender.OnWindowResized(windowWidth, windowHeight);

    g_audioDriver.Initialize();

    if (!g_client->Init(engineService, argc, argv)) {
        return false;
    }

    RenderContext renderContext{};

    float CpuCyclesPerSec = 1'500'000;
    // float PsgCyclesPerSec = CpuCyclesPerSec / 16;
    // float PsgCyclesPerAudioSample = PsgCyclesPerSec / g_audioDriver.GetSampleRate();
    float CpuCyclesPerAudioSample = CpuCyclesPerSec / g_audioDriver.GetSampleRate();
    AudioContext audioContext{CpuCyclesPerAudioSample};

    bool quit = false;
    while (!quit) {
        PollEvents(quit);
        UpdatePauseState(g_paused[PauseSource::Game]);
        auto input = UpdateInput();
        const auto frameTime = UpdateFrameTime();

        auto emuEvents = EmuEvents{};
        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_C).down) {
            emuEvents.push_back({EmuEvent::BreakIntoDebugger{}});

            // @HACK: Because the SDL window ends up losing focus to the console window, we
            // don't get the KEY_UP events for these keys right away, so "continuing" from the
            // console ends up breaking back into it again. We avoid this by explicitly clearing
            // the state of these keys.
            g_keyboard.ResetKeyState(SDL_SCANCODE_LCTRL);
            g_keyboard.ResetKeyState(SDL_SCANCODE_C);
        }

        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_R).pressed) {
            emuEvents.push_back({EmuEvent::Reset{}});
        }

        if (g_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
            g_keyboard.GetKeyState(SDL_SCANCODE_O).pressed) {
            emuEvents.push_back({EmuEvent::OpenRomFile{}});
        }

        ImGui_ImplSdlGL3_NewFrame(g_window);

        UpdateMenu(quit, emuEvents);

        HACK_Simulate3dImager(frameTime, input);

        if (!g_client->FrameUpdate(frameTime, {std::ref(emuEvents), std::ref(g_options)}, input,
                                   renderContext, audioContext))
            quit = true;

        // Audio update
        g_audioDriver.AddSamples(audioContext.samples.data(), audioContext.samples.size());
        audioContext.samples.clear();
        g_audioDriver.Update(frameTime);

        // Render update
        const size_t MaxLinesInTurboMode = 1'000;
        if (IsTurboMode() && renderContext.lines.size() > MaxLinesInTurboMode) {
            renderContext.lines.resize(MaxLinesInTurboMode);
        }

        g_glRender.RenderScene(frameTime, renderContext);
        ImGui_Render();
        SDL_GL_SwapWindow(g_window);

        // Don't clear lines when paused
        if (frameTime > 0) {
            renderContext.lines.clear();
        }

        g_keyboard.PostFrameUpdateKeyStates();
        g_controllerDriver.PostFrameUpdateKeyStates();
    }

    g_client->Shutdown();

    g_audioDriver.Shutdown();
    g_glRender.Shutdown();
    ImGui_ImplSdlGL3_Shutdown();

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
                g_glRender.OnWindowResized(width, height);
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
            g_controllerDriver.AddController(sdlEvent.cdevice.which);
            break;

        case SDL_CONTROLLERDEVICEREMOVED:
            g_controllerDriver.RemoveController(sdlEvent.cdevice.which);
            break;

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP: {
            const auto& cbutton = sdlEvent.cbutton;
            const bool buttonDown = sdlEvent.type == SDL_CONTROLLERBUTTONDOWN;
            g_controllerDriver.ControllerByInstanceId(sdlEvent.cbutton.which)
                .OnButtonStateChange(cbutton.button, buttonDown);
        } break;

        case SDL_CONTROLLERAXISMOTION: {
            const auto& caxis = sdlEvent.caxis;
            g_controllerDriver.ControllerByInstanceId(sdlEvent.cdevice.which)
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
    g_frameTimer.FrameUpdate();
    double frameTime = g_frameTimer.GetFrameTime();

    // Reset to 0 if paused
    if (IsPaused())
        frameTime = 0.0;

    // Scale up if turbo
    UpdateTurboMode();
    if (IsTurboMode())
        frameTime *= 10;

    return frameTime;
}

void SDLEngine::UpdateMenu(bool& quit, EmuEvents& emuEvents) {
    // ImGui menu bar
    if (ImGui::BeginMainMenuBar()) {
        g_paused[PauseSource::Menu] = false;
        bool openAboutDialog = false;

        if (ImGui::BeginMenu("File")) {
            g_paused[PauseSource::Menu] = true;

            if (ImGui::MenuItem("Open rom...", "Ctrl+O"))
                emuEvents.push_back({EmuEvent::OpenRomFile{}});

            if (auto lastOpenedFile = g_options.Get<std::string>("lastOpenedFile");
                !lastOpenedFile.empty()) {

                auto filename = fs::path(lastOpenedFile).filename().replace_extension().string();
                if (ImGui::MenuItem(FormattedString<>("Open recent: %s", filename.c_str()))) {
                    emuEvents.push_back({EmuEvent::OpenRomFile{lastOpenedFile}});
                }
            }

            if (ImGui::MenuItem("Exit"))
                quit = true;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Emulation")) {
            g_paused[PauseSource::Menu] = true;

            if (ImGui::MenuItem("Reset", "Ctrl+R"))
                emuEvents.push_back({EmuEvent::Reset{}});

            ImGui::MenuItem("Pause", "P", &g_paused[PauseSource::Game]);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            g_paused[PauseSource::Menu] = true;

#ifdef DEBUG_UI_ENABLED
            ImGui::MenuItem("Debug window", "", &Gui::EnabledWindows[Gui::Window::Debug]);
#endif

            if (ImGui::MenuItem("Break into Debugger", "Ctrl+C"))
                emuEvents.push_back({EmuEvent::BreakIntoDebugger{}});

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
        const double fps = g_frameTimer.GetFps();
        RightAlignLabelText(FormattedString<>("%d FPS (%.3f ms)", static_cast<int>(fps + 0.5),
                                              fps > 0 ? (1000.f / fps) : 0.f));

        ImGui::EndMainMenuBar();

        // Handle popups

        if (openAboutDialog) {
            ImGui::OpenPopup("About Vectrexy");
        }
        if (bool open = true;
            ImGui::BeginPopupModal("About Vectrexy", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
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

#ifdef DEBUG_UI_ENABLED
    //@TODO: save imguiEnabledWindows#Name and just iterate here
    static auto lastEnabledWindows = Gui::EnabledWindows;
    if (Gui::EnabledWindows[Gui::Window::Debug] != lastEnabledWindows[Gui::Window::Debug]) {
        g_options.Set("imguiDebugWindow", Gui::EnabledWindows[Gui::Window::Debug]);
        g_options.Save();
    }
    lastEnabledWindows = Gui::EnabledWindows;
#endif
}
