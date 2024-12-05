#include "sdl_engine/SDLEngine.h"

#include "GLRender.h"
#include "GLUtil.h"
#include "InputManager.h"
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
#include "engine/EngineUtil.h"
#include "engine/Options.h"
#include "engine/Paths.h"
#include "imgui_impl/imgui_impl_sdl_gl3.h"
#include <SDL.h>
#include <SDL_net.h>
#include <algorithm>
#include <fstream>
#include <iostream>

#ifdef PLATFORM_WINDOWS
#include <fcntl.h> // _O_BINARY
#include <io.h>    // _setmode
#endif

// Include SDL_syswm.h for SDL_GetWindowWMInfo
// This includes windows.h on Windows platforms, we have to do the usual dance of disabling certain
// warnings and undef'ing certain macros
// @TODO: #include "PlatformHeaders.h" that abstracts this?
MSC_PUSH_WARNING_DISABLE(4121)
#include <SDL_syswm.h>
#undef min
#undef max
MSC_POP_WARNING_DISABLE()

extern void ImGui_ImplSdlGL3_RenderDrawLists(ImDrawData*);

namespace {
    // Display window dimensions
    const int DEFAULT_WINDOW_WIDTH = 600;
    inline int WindowHeightFromWidth(int width) {
        return static_cast<int>(width * 4.0f / 3.0f);
    }
    inline int WindowWidthFromHeight(int height) {
        return static_cast<int>(height * 3.0f / 4.0f);
    }

    namespace PauseSource {
        enum Type { Game, Menu, Size };
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
        ImGui_ImplSdlGL3_RenderDrawLists(ImGui::GetDrawData());
    }

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

    void SetVSyncEnabled(bool enabled) {
        enum SwapInterval : int { NoVSync = 0, VSync = 1, AdaptiveVSync = -1 };
        if (enabled) {
            // Prefer adaptive if available
            if (SDL_GL_SetSwapInterval(SwapInterval::AdaptiveVSync) == -1) {
                SDL_GL_SetSwapInterval(SwapInterval::VSync);
            }
        } else {
            SDL_GL_SetSwapInterval(SwapInterval::NoVSync);
        }
    }
} // namespace

class SDLEngineImpl {
public:
    void RegisterClient(IEngineClient& client) { m_client = &client; }

    std::string GetVersionString() const {
        if (auto fin = std::ifstream("version.txt"); fin) {
            std::string version;
            fin >> version;
            return version;
        }
        return "(Dev)";
    }

    bool Run(int argc, char** argv) {
        const auto args = std::vector<std::string_view>(argv + 1, argv + argc);

        if (!EngineUtil::FindAndSetRootPath(fs::path(fs::absolute(argv[0])))) {
            Errorf("Failed to find and set root path\n");
            return false;
        }

        // Create standard directories
        fs::create_directories(Paths::overlaysDir);
        fs::create_directories(Paths::romsDir);
        fs::create_directories(Paths::userDir);
        fs::create_directories(Paths::devDir);

        if (contains(args, "-dap")) {
            // TODO: move to DapDebugger or Platform
#ifdef PLATFORM_WINDOWS
            // Change stdin and stdout from text mode to binary mode.
            // This ensures sequences of \r\n are not changed to \n.
            _setmode(_fileno(stdin), _O_BINARY);
            _setmode(_fileno(stdout), _O_BINARY);
#endif
            // DAP uses stdin/stdout to communicate, so route all our printing to a file
            FILE* fs = fopen((Paths::devDir / "print_stream.txt").string().c_str(),
                             "w+"); // Leak it for now
            SetStream(ConsoleStream::Output, fs);
            SetStream(ConsoleStream::Error, fs);
            SetStreamAutoFlush(true);
        }

        std::shared_ptr<IEngineService> engineService =
            std::make_shared<aggregate_adapter<IEngineService>>(
                // SetFocusMainWindow
                [this] { Platform::SetFocus(GetMainWindowHandle()); },
                // SetFocusConsole
                [] { Platform::SetConsoleFocus(); },
                // ResetOverlay
                [this](const char* file) { m_glRender.ResetOverlay(file); });

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
            std::cout << "SDL cannot init with error " << SDL_GetError() << std::endl;
            return false;
        }

        if (SDLNet_Init() < 0) {
            std::cout << "SDLNet failed to init with error " << SDL_GetError() << std::endl;
            return false;
        }

        m_inputManager.Init(m_keyboard, m_controllerDriver);

        // Load options
        m_options.Add<std::string>("biosRomFile", Paths::biosRomFile.string());
        m_options.Add<int>("windowX", -1);
        m_options.Add<int>("windowY", -1);
        m_options.Add<int>("windowWidth", -1);
        m_options.Add<int>("windowHeight", -1);
        m_options.Add<bool>("windowMaximized", false);
        m_options.Add<bool>("imguiDebugWindow", false);
        m_options.Add<bool>("enableGLDebugging", false);
        m_options.Add<float>("imguiFontScale", GetDefaultImguiFontScale());
        m_options.Add<std::string>("lastOpenedFile", {});
        m_options.Add<float>("volume", 0.5f);
        m_options.Add<bool>("vsync", false);
        m_options.Add<float>("brightnessCurve", 0.0f);
        m_inputManager.AddOptions(m_options);
        m_options.SetFilePath(Paths::optionsFile);
        m_options.Load();

        // Init input mapping and device for each player from options
        m_inputManager.ReadOptions(m_options);

        int windowX = m_options.Get<int>("windowX");
        if (windowX == -1)
            windowX = SDL_WINDOWPOS_CENTERED;

        int windowY = m_options.Get<int>("windowY");
        if (windowY == -1)
            windowY = SDL_WINDOWPOS_CENTERED;

        int windowWidth = m_options.Get<int>("windowWidth");
        int windowHeight = m_options.Get<int>("windowHeight");
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
        if (m_options.Get<bool>("windowMaximized"))
            windowCreateFlags |= SDL_WINDOW_MAXIMIZED;

        std::string windowTitle = std::string{"Vectrexy"} + " (" + GetVersionString() + ")";
        m_window = SDL_CreateWindow(windowTitle.c_str(), windowX, windowY, windowWidth,
                                    windowHeight, windowCreateFlags);
        if (m_window == nullptr) {
            std::cout << "Cannot create window with error " << SDL_GetError() << std::endl;
            return false;
        }

        const bool enableGLDebugging = m_options.Get<bool>("enableGLDebugging");

        m_glContext = CreateGLContext(m_window, enableGLDebugging);
        if (m_glContext == nullptr) {
            std::cout << "Cannot create OpenGL context with error " << SDL_GetError() << std::endl;
            return false;
        }

        SetVSyncEnabled(m_options.Get<bool>("vsync"));

#ifdef DEBUG_UI_ENABLED
        Gui::EnabledWindows[Gui::Window::Debug] = m_options.Get<bool>("imguiDebugWindow");
#endif

        ImGui_ImplSdlGL3_Init(m_window);
        ImGui::GetIO().FontGlobalScale = m_options.Get<float>("imguiFontScale");
        static const auto imguiIniFilePath = Paths::imguiIniFile.string();
        ImGui::GetIO().IniFilename = imguiIniFilePath.c_str();

        m_glRender.Initialize(enableGLDebugging);
        m_glRender.OnWindowResized(windowWidth, windowHeight);

        m_audioDriver.Initialize();
        m_audioDriver.SetVolume(m_options.Get<float>("volume"));

        if (!m_client->Init(args, engineService, m_options.Get<std::string>("biosRomFile"))) {
            return false;
        }

        RenderContext renderContext{};

        float CpuCyclesPerSec = 1'500'000;
        // float PsgCyclesPerSec = CpuCyclesPerSec / 16;
        // float PsgCyclesPerAudioSample = PsgCyclesPerSec / m_audioDriver.GetSampleRate();
        float CpuCyclesPerAudioSample = CpuCyclesPerSec / m_audioDriver.GetSampleRate();
        AudioContext audioContext{CpuCyclesPerAudioSample};

        bool quit = false;
        while (!quit) {
            PollEvents(quit);
            UpdatePauseState(m_paused[PauseSource::Game]);
            auto input = UpdateInput();
            const auto frameTime = UpdateFrameTime();

            auto emuEvents = EmuEvents{};
            if (m_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
                m_keyboard.GetKeyState(SDL_SCANCODE_C).down) {
                emuEvents.push_back({EmuEvent::BreakIntoDebugger{}});

                // @HACK: Because the SDL window ends up losing focus to the console window, we
                // don't get the KEY_UP events for these keys right away, so "continuing" from the
                // console ends up breaking back into it again. We avoid this by explicitly clearing
                // the state of these keys.
                m_keyboard.ResetKeyState(SDL_SCANCODE_LCTRL);
                m_keyboard.ResetKeyState(SDL_SCANCODE_C);
            }

            if (m_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
                m_keyboard.GetKeyState(SDL_SCANCODE_R).pressed) {
                emuEvents.push_back({EmuEvent::Reset{}});
            }

            if (m_keyboard.GetKeyState(SDL_SCANCODE_LCTRL).down &&
                m_keyboard.GetKeyState(SDL_SCANCODE_O).pressed) {
                emuEvents.push_back({EmuEvent::OpenRomFile{}});
            }

            ImGui_ImplSdlGL3_NewFrame(m_window);

            UpdateMenu(quit, emuEvents);

            HACK_Simulate3dImager(frameTime, input);

            if (!m_client->FrameUpdate(frameTime, {std::ref(emuEvents), std::ref(m_options)}, input,
                                       renderContext, audioContext)) {
                quit = true;
            }

            // Audio update
            m_audioDriver.AddSamples(audioContext.samples.data(), audioContext.samples.size());
            audioContext.samples.clear();
            m_audioDriver.Update(frameTime);

            // Render update
            const size_t MaxLinesInTurboMode = 1'000;
            if (IsTurboMode() && renderContext.lines.size() > MaxLinesInTurboMode) {
                renderContext.lines.resize(MaxLinesInTurboMode);
            }

            m_glRender.RenderScene(frameTime, renderContext);
            ImGui_Render();
            SDL_GL_SwapWindow(m_window);

            // Don't clear lines when paused
            if (frameTime > 0) {
                renderContext.lines.clear();
            }

            m_keyboard.PostFrameUpdateKeyStates();
            m_controllerDriver.PostFrameUpdateKeyStates();
        }

        m_client->Shutdown();

        m_audioDriver.Shutdown();
        m_glRender.Shutdown();
        ImGui_ImplSdlGL3_Shutdown();

        SDL_DestroyWindow(m_window);
        SDLNet_Quit();
        SDL_Quit();

        return true;
    }

private:
    // template <typename InputDeviceType>
    // void SetInputDevice(int player) {
    //    if constexpr (std::is_same_v<InputDeviceType, NullInputDevice>) {
    //        m_inputDevices[player] = NullInputDevice{};
    //    } else if constexpr (std::is_same_v<InputDeviceType, KeyboardInputDevice>) {
    //        m_inputDevices[player] = KeyboardInputDevice{m_keyboard, player};
    //    } else if constexpr (std::is_same_v<InputDeviceType, GamepadInputDevice>) {
    //        m_inputDevices[player] = GamepadInputDevice{m_controllerDriver, player};
    //    } else {
    //        static_assert(false);
    //    }
    //}

    // void SetInputDeviceByIndex(size_t inputDeviceIndex, int player) {
    //    switch (inputDeviceIndex) {
    //    case InputDevice::IndexOf<NullInputDevice>:
    //        SetInputDevice<NullInputDevice>(player);
    //        break;
    //    case InputDevice::IndexOf<KeyboardInputDevice>:
    //        SetInputDevice<KeyboardInputDevice>(player);
    //        break;

    //    case InputDevice::IndexOf<GamepadInputDevice>:
    //        SetInputDevice<GamepadInputDevice>(player);
    //        break;
    //    default:
    //        assert(false);
    //    }
    //}

    void PollEvents(bool& quit) {
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
                        m_options.Set("windowWidth", width);
                        m_options.Set("windowHeight", height);
                    }
                    m_options.Set("windowMaximized", IsWindowMaximized());
                    m_options.Save();
                    m_glRender.OnWindowResized(width, height);
                } break;

                case SDL_WINDOWEVENT_MOVED: {
                    const int x = sdlEvent.window.data1;
                    const int y = sdlEvent.window.data2;
                    if (!IsWindowMaximized()) {
                        m_options.Set("windowX", x);
                        m_options.Set("windowY", y);
                    }
                    m_options.Set("windowMaximized", IsWindowMaximized());
                    m_options.Save();
                }
                }
                break;

            case SDL_CONTROLLERDEVICEADDED:
                m_controllerDriver.AddController(sdlEvent.cdevice.which);
                break;

            case SDL_CONTROLLERDEVICEREMOVED:
                m_controllerDriver.RemoveController(sdlEvent.cdevice.which);
                break;

            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                const auto& cbutton = sdlEvent.cbutton;
                const bool buttonDown = sdlEvent.type == SDL_CONTROLLERBUTTONDOWN;
                m_controllerDriver.ControllerByInstanceId(sdlEvent.cbutton.which)
                    .OnButtonStateChange(cbutton.button, buttonDown);
            } break;

            case SDL_CONTROLLERAXISMOTION: {
                const auto& caxis = sdlEvent.caxis;
                m_controllerDriver.ControllerByInstanceId(sdlEvent.cdevice.which)
                    .OnAxisStateChange(caxis.axis, caxis.value);
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                m_keyboard.OnKeyStateChange(sdlEvent.key);
                break;
            }
        }
    }

    double UpdateFrameTime() {
        m_frameTimer.FrameUpdate();
        double frameTime = m_frameTimer.GetFrameTime();

        // Reset to 0 if paused
        if (IsPaused())
            frameTime = 0.0;

        // Scale up if turbo
        UpdateTurboMode();
        if (IsTurboMode())
            frameTime *= 10;

        return frameTime;
    }

    void UpdateMenu(bool& quit, EmuEvents& emuEvents) {
        // ImGui menu bar
        if (ImGui::BeginMainMenuBar()) {
            m_paused[PauseSource::Menu] = false;
            bool openAboutPopup = false;
            bool openConfigureInputPopup = false;

            if (ImGui::BeginMenu("File")) {
                m_paused[PauseSource::Menu] = true;

                if (ImGui::MenuItem("Open rom...", "Ctrl+O"))
                    emuEvents.push_back({EmuEvent::OpenRomFile{}});

                if (auto lastOpenedFile = m_options.Get<std::string>("lastOpenedFile");
                    !lastOpenedFile.empty()) {

                    auto filename =
                        fs::path(lastOpenedFile).filename().replace_extension().string();
                    if (ImGui::MenuItem(FormattedString<>("Open recent: %s", filename.c_str()))) {
                        emuEvents.push_back({EmuEvent::OpenRomFile{lastOpenedFile}});
                    }
                }

                if (ImGui::MenuItem("Exit"))
                    quit = true;

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Emulation")) {
                m_paused[PauseSource::Menu] = true;

                if (ImGui::MenuItem("Reset", "Ctrl+R"))
                    emuEvents.push_back({EmuEvent::Reset{}});

                ImGui::MenuItem("Pause", "P", &m_paused[PauseSource::Game]);
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Settings")) {
                ImGui::Text("System");
                // Bios
                {
                    static const std::array<const char*, 3> items{"Normal", "Fast", "Skip"};
                    static const std::array<fs::path, 3> biosFiles{
                        Paths::biosRomFile, Paths::biosRomFastFile, Paths::biosRomSkipFile};

                    auto currBiosFile = m_options.Get<std::string>("biosRomFile");
                    int index = find_index_of(biosFiles, currBiosFile, 0);
                    if (ImGui::Combo("Bios", &index, items.data(), (int)items.size())) {
                        emuEvents.push_back({EmuEvent::OpenBiosRomFile{biosFiles[index]}});
                    }
                }

                ImGui::Separator();
                ImGui::Text("Display");
                static bool vsync = m_options.Get<bool>("vsync");
                ImGui::Checkbox("VSync", &vsync);
                if (vsync != m_options.Get<bool>("vsync")) {
                    SetVSyncEnabled(vsync);
                    m_options.Set("vsync", vsync);
                    m_options.Save();
                }

                static float brightnessCurve = m_options.Get<float>("brightnessCurve");
                ImGui::SliderFloat("Brightness curve", &brightnessCurve, 0.f, 1.f);
                if (brightnessCurve != m_options.Get<float>("brightnessCurve")) {
                    // Just update the value in the options file. This value must be polled by the
                    // engine client.
                    m_options.Set("brightnessCurve", brightnessCurve);
                    m_options.Save();
                }

                ImGui::Separator();
                ImGui::Text("Sound");

                static float volume = m_options.Get<float>("volume");
                ImGui::SliderFloat("Volume", &volume, 0.f, 1.f);
                if (volume != m_options.Get<float>("volume")) {
                    m_audioDriver.SetVolume(volume);
                    m_options.Set("volume", volume);
                    m_options.Save();
                }

                ImGui::Separator();
                ImGui::Text("Input");
                if (ImGui::Button("Configure...")) {
                    openConfigureInputPopup = true;
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Debug")) {
                m_paused[PauseSource::Menu] = true;

#ifdef DEBUG_UI_ENABLED
                ImGui::MenuItem("Debug window", "", &Gui::EnabledWindows[Gui::Window::Debug]);
#endif

                if (ImGui::MenuItem("Break into Debugger", "Ctrl+C"))
                    emuEvents.push_back({EmuEvent::BreakIntoDebugger{}});

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Help")) {
                m_paused[PauseSource::Menu] = true;

                if (ImGui::MenuItem("About Vectrexy"))
                    openAboutPopup = true;
                ImGui::EndMenu();
            }

            auto RightAlignLabelText = [](const char* text) {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(text).x);
                ImGui::LabelText("", text);
            };
            const double fps = m_frameTimer.GetFps();
            RightAlignLabelText(FormattedString<>("%d FPS (%.3f ms)", static_cast<int>(fps + 0.5),
                                                  fps > 0 ? (1000.f / fps) : 0.f));

            ImGui::EndMainMenuBar();

            // Handle popups
            UpdateMenu_AboutPopup(openAboutPopup);
            UpdateMenu_ConfigureInputPopup(openConfigureInputPopup);
        }

#ifdef DEBUG_UI_ENABLED
        //@TODO: save imguiEnabledWindows#Name and just iterate here
        static auto lastEnabledWindows = Gui::EnabledWindows;
        if (Gui::EnabledWindows[Gui::Window::Debug] != lastEnabledWindows[Gui::Window::Debug]) {
            m_options.Set("imguiDebugWindow", Gui::EnabledWindows[Gui::Window::Debug]);
            m_options.Save();
        }
        lastEnabledWindows = Gui::EnabledWindows;
#endif
    }

    void UpdateMenu_AboutPopup(bool openPopup) {
        if (openPopup) {
            ImGui::OpenPopup("About Vectrexy");
        }
        if (bool open = true;
            ImGui::BeginPopupModal("About Vectrexy", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_paused[PauseSource::Menu] = true;

            ImGui::Text("Vectrexy (%s)", GetVersionString().c_str());
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

    void UpdateMenu_ConfigureInputPopup(bool openPopup) {
        const char* popupName = "Configure Input";

        if (openPopup) {
            ImGui::OpenPopup(popupName);
        }
        if (bool open = true;
            ImGui::BeginPopupModal(popupName, &open, ImGuiWindowFlags_AlwaysAutoResize)) {
            m_paused[PauseSource::Menu] = true;

            auto ConfigureInputForPlayer = [&](int player) {
                int deviceIndex = m_inputManager.GetInputDeviceIndex(player);

                bool openSetupKeyboardPopup = false;
                bool openSetupGamepadPopup = false;

                // User can change device
                const auto lastDeviceIndex = deviceIndex;
                ImGui::ListBox(FormattedString<>("Player %d Device", player + 1), &deviceIndex,
                               &InputDevice::Name[0], (int)InputDevice::Name.size());

                if (lastDeviceIndex != deviceIndex) {
                    m_inputManager.SetInputDeviceByIndex(deviceIndex, player);
                    m_inputManager.WriteOptions(m_options);
                    m_options.Save();
                }

                // User can configure currently selected device mapping via popup
                if (deviceIndex == InputDevice::IndexOf<KeyboardInputDevice>) {
                    if (ImGui::Button(FormattedString<>("Setup Player %d keyboard", player + 1))) {
                        openSetupKeyboardPopup = true;
                    }
                } else if (deviceIndex == InputDevice::IndexOf<GamepadInputDevice>) {
                    if (ImGui::Button(FormattedString<>("Setup Player %d gamepad", player + 1))) {
                        openSetupGamepadPopup = true;
                    }
                }

                UpdateMenu_ConfigureInputPopup_SetupKeyboard(openSetupKeyboardPopup, player);
            };

            ConfigureInputForPlayer(0);
            ConfigureInputForPlayer(1);

            ImGui::EndPopup();
        }
    }

    void UpdateMenu_ConfigureInputPopup_SetupKeyboard(bool openPopup, int player) {
        const auto popupName = std::string("Set Keys for Player ") + std::to_string(player + 1);

        static KeyboardInputMapping::Type nextKeyToSet{};
        static KeyboardInputMapping::KeyToScancodeArray newKeys{};

        const KeyboardInputMapping::KeyToScancodeArray& keys =
            m_inputManager.GetInputMapping(player).keyboard.keys;

        if (openPopup) {
            ImGui::OpenPopup(popupName.c_str());
            nextKeyToSet = static_cast<KeyboardInputMapping::Type>(0);
            newKeys = keys;
        }
        if (bool open = true;
            ImGui::BeginPopupModal(popupName.c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize)) {

            const std::string currKeyName = [&]() -> std::string {
                std::string name = SDL_GetScancodeName(keys[nextKeyToSet]);
                if (!name.empty())
                    return name;
                return FormattedString<>("<Scancode %d>", keys[nextKeyToSet]).Value();
            }();

            ImGui::Text(FormattedString<>("Input: '%s' mapped to key: '%s'\n\nPress a new key "
                                          "or Escape to keep current.\n\n",
                                          KeyboardInputMapping::Name[nextKeyToSet],
                                          currKeyName.c_str()));

            if (ImGui::Button("Reset all to default")) {
                newKeys = KeyboardInputMapping::DefaultKeys[player];
                nextKeyToSet = KeyboardInputMapping::Count;
            } else if (auto scancode = m_keyboard.GetLastKeyPressed()) {
                if (scancode != SDL_SCANCODE_ESCAPE) {
                    newKeys[nextKeyToSet] = *scancode;
                }
                nextKeyToSet = static_cast<KeyboardInputMapping::Type>(nextKeyToSet + 1);
            }

            if (nextKeyToSet == KeyboardInputMapping::Count) {
                m_inputManager.GetInputMapping(player).keyboard.keys = newKeys;
                m_inputManager.WriteOptions(m_options);
                m_options.Save();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    Platform::WindowHandle GetMainWindowHandle() {
        SDL_SysWMinfo info{};
        SDL_GetWindowWMInfo(m_window, &info);
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
        return info.info.win.window;
#else
        return {};
#endif
    }

    bool IsWindowMaximized() { return (SDL_GetWindowFlags(m_window) & SDL_WINDOW_MAXIMIZED) != 0; }

    SDL_GLContext CreateGLContext(SDL_Window* window, bool enableGLDebugging) {
        auto [major, minor] = m_glRender.GetMajorMinorVersion();
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

    Input UpdateInput() { return m_inputManager.Poll(); }

    void UpdatePauseState(bool& paused) {
        bool togglePause = false;

        if (m_keyboard.GetKeyState(SDL_SCANCODE_P).pressed) {
            togglePause = true;
        }

        for (int i = 0; i < m_controllerDriver.NumControllers(); ++i) {
            auto& controller = m_controllerDriver.ControllerByIndex(i);
            if (controller.GetButtonState(SDL_CONTROLLER_BUTTON_START).pressed)
                togglePause = true;
        }

        if (togglePause)
            paused = !paused;
    }

    bool IsPaused() {
        for (auto& v : m_paused) {
            if (v)
                return true;
        }
        return false;
    }

    void UpdateTurboMode() {
        m_turbo = false;

        if (m_keyboard.GetKeyState(SDL_SCANCODE_GRAVE).down) {
            m_turbo = true;
        }

        for (int i = 0; i < m_controllerDriver.NumControllers(); ++i) {
            auto& controller = m_controllerDriver.ControllerByIndex(i);
            if (controller.GetAxisValue(SDL_CONTROLLER_AXIS_RIGHTX) > 16000)
                m_turbo = true;
        }
    }

    bool IsTurboMode() { return m_turbo; }

    IEngineClient* m_client = nullptr;
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext{};
    GLRender m_glRender;
    SDLGameControllerDriver m_controllerDriver;
    SDLKeyboard m_keyboard;
    SDLAudioDriver m_audioDriver;
    InputManager m_inputManager;
    Options m_options;
    FrameTimer m_frameTimer;
    bool m_paused[PauseSource::Size]{};
    bool m_turbo = false;
};

SDLEngine::SDLEngine() = default;
SDLEngine::~SDLEngine() = default;

void SDLEngine::RegisterClient(IEngineClient& client) {
    m_impl->RegisterClient(client);
}

bool SDLEngine::Run(int argc, char** argv) {
    return m_impl->Run(argc, argv);
}
