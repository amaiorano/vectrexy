#include "core/Base.h"
#include "core/Platform.h"
#include "debugger/DapDebugger.h"
#include "debugger/Debugger.h"
#include "emulator/Emulator.h"
#include "engine/EngineClient.h"
#include "engine/Overlays.h"
#include "engine/Paths.h"
#include <memory>

#if defined(ENGINE_NULL)
#include "null_engine/NullEngine.h"
using Engine = NullEngine;
#elif defined(ENGINE_SDL)
#include "sdl_engine/SDLEngine.h"
using Engine = SDLEngine;
#else
#error Define Engine type for this platform
#endif

class EngineClient final : public IEngineClient {
private:
    bool Init(const std::vector<std::string_view>& args,
              std::shared_ptr<IEngineService>& engineService,
              std::string_view biosRomFile) override {
        m_engineService = engineService;

        m_overlays.LoadOverlays(Paths::overlaysDir);

        //@TODO: Clean this up
        std::string rom = "";
        for (auto& arg : args) {
            if (arg[0] != '-')
                rom = arg;
        }

        //@TODO: polymorphic IDebugger base class
        if (contains(args, "-dap")) {
            m_dapDebugger = std::make_unique<DapDebugger>();
        } else {
            m_debugger = std::make_unique<Debugger>();
        }

        m_emulator.Init(biosRomFile.data());

        if (m_debugger)
            m_debugger->Init(args, engineService, Paths::devDir, m_emulator);

        if (m_dapDebugger)
            m_dapDebugger->Init(Paths::devDir, m_emulator);

        if (!rom.empty()) {
            LoadRom(rom.c_str());
        } else {
            // If no rom is loaded, we'll play the built-in Mine Storm
            ResetOverlay("Minestorm");
        }

        Reset();

        return true;
    }

    void Reset() {
        m_emulator.Reset();
        if (m_debugger)
            m_debugger->Reset();
        if (m_dapDebugger)
            m_dapDebugger->Reset();
        ErrorHandler::Reset();
    }

    bool LoadRom(const char* file) {
        if (!m_emulator.LoadRom(file)) {
            Errorf("Failed to load rom file: %s\n", file);
            return false;
        }

        if (m_dapDebugger)
            m_dapDebugger->OnRomLoaded(file);

        //@TODO: Show game name in title bar

        ResetOverlay(file);

        return true;
    }

    void ResetOverlay(const char* file) {
        auto overlayPath = m_overlays.FindOverlay(file);
        if (overlayPath) {
            auto path = overlayPath->string();
            Errorf("Found overlay for %s: %s\n", file, path.c_str());
            m_engineService->ResetOverlay(path.c_str());
        } else {
            Errorf("No overlay found for %s\n", file);
            m_engineService->ResetOverlay(nullptr);
        }
    }

    bool FrameUpdate(double frameTime, const EmuContext& emuContext, const Input& input,
                     RenderContext& renderContext, AudioContext& audioContext) override {
        EmuEvents& emuEvents = emuContext.emuEvents;
        Options& options = emuContext.options;

        for (auto& event : emuEvents) {
            if (auto reset = std::get_if<EmuEvent::Reset>(&event.type)) {
                Reset();

            } else if (auto openBiosRomFile = std::get_if<EmuEvent::OpenBiosRomFile>(&event.type)) {
                auto biosRomPath = openBiosRomFile->path.string();
                if (m_emulator.LoadBios(biosRomPath.c_str())) {
                    options.Set("biosRomFile", biosRomPath);
                    options.Save();
                    Reset(); // TODO: Ask user?
                }

            } else if (auto openRomFile = std::get_if<EmuEvent::OpenRomFile>(&event.type)) {
                fs::path romPath{};
                if (openRomFile->path.empty()) {
                    fs::path lastOpenedFile = options.Get<std::string>("lastOpenedFile");

                    auto result = Platform::OpenFileDialog(
                        "Open Vectrex rom", "Vectrex Rom", "*.vec;*.bin",
                        lastOpenedFile.empty() ? Paths::romsDir : lastOpenedFile);

                    if (result)
                        romPath = *result;
                } else {
                    romPath = openRomFile->path;
                }

                if (!romPath.empty() && LoadRom(romPath.string().c_str())) {
                    options.Set("lastOpenedFile", romPath.string());
                    options.Save();
                    Reset();
                }
            }
        }

        bool keepGoing = false;
        if (m_debugger) {
            keepGoing =
                m_debugger->FrameUpdate(frameTime, emuEvents, input, renderContext, audioContext);
        }

        if (m_dapDebugger) {
            keepGoing = m_dapDebugger->FrameUpdate(frameTime, emuEvents, input, renderContext,
                                                   audioContext);
        }

        // Send option-based values
        m_emulator.GetVia().GetScreen().SetBrightnessCurve(options.Get<float>("brightnessCurve"));

        m_emulator.FrameUpdate(frameTime);

        return keepGoing;
    }

    void Shutdown() override {}

    std::shared_ptr<IEngineService> m_engineService;
    Emulator m_emulator;
    std::unique_ptr<Debugger> m_debugger;
    std::unique_ptr<DapDebugger> m_dapDebugger;
    Overlays m_overlays;
};

int main(int argc, char** argv) {
    auto client = std::make_unique<EngineClient>();
    auto engine = std::make_unique<Engine>();
    engine->RegisterClient(*client);
    bool result = engine->Run(argc, argv);
    return result ? 0 : -1;
}
