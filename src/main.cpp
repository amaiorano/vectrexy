#include "EngineClient.h"
#include "Overlays.h"
#include "SDLEngine.h"
#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/FileSystemUtil.h"
#include "core/Platform.h"
#include "debugger/Debugger.h"
#include "emulator/Emulator.h"
#include <memory>
#include <random>

class Vectrexy final : public IEngineClient {
private:
    bool Init(int argc, char** argv) override {
        m_overlays.LoadOverlays();

        //@TODO: Clean this up
        std::string rom = "";
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg[0] != '-')
                rom = arg;
        }

        m_emulator.Init();
        m_debugger.Init(argc, argv, m_emulator);

        if (!rom.empty()) {
            LoadRom(rom.c_str());
        } else {
            // If no rom is loaded, we'll play the built-in Mine Storm
            LoadOverlay("Minestorm");
        }

        Reset();

        return true;
    }

    void Reset() {
        m_emulator.Reset();
        m_debugger.Reset();
    }

    bool LoadRom(const char* file) {
        if (!m_emulator.LoadRom(file)) {
            Errorf("Failed to load rom file: %s\n", file);
            return false;
        }

        //@TODO: Show game name in title bar

        LoadOverlay(file);

        return true;
    }

    void LoadOverlay(const char* file) {
        auto overlayPath = m_overlays.FindOverlay(file);
        if (overlayPath) {
            auto path = overlayPath->string();
            Errorf("Found overlay for %s: %s\n", file, path.c_str());
            ResetOverlay(path.c_str());
        } else {
            Errorf("No overlay found for %s\n", file);
            ResetOverlay();
        }
    }

    bool FrameUpdate(double frameTime, const Input& input, const EmuContext& emuContext,
                     RenderContext& renderContext, AudioContext& audioContext) override {
        EmuEvents& emuEvents = emuContext.emuEvents;
        Options& options = emuContext.options;

        for (auto& event : emuEvents) {
            if (auto reset = std::get_if<EmuEvent::Reset>(&event.type)) {
                Reset();
            } else if (auto openRomFile = std::get_if<EmuEvent::OpenRomFile>(&event.type)) {
                fs::path romPath{};
                if (openRomFile->path.empty()) {
                    fs::path lastOpenedFile = options.Get<std::string>("lastOpenedFile");

                    auto result =
                        Platform::OpenFileDialog("Open Vectrex rom", "Vectrex Rom", "*.vec;*.bin",
                                                 lastOpenedFile.empty() ? "roms" : lastOpenedFile);

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

        bool keepGoing =
            m_debugger.FrameUpdate(frameTime, input, emuEvents, renderContext, audioContext);

        m_emulator.FrameUpdate(frameTime);

        return keepGoing;
    }

    void Shutdown() override {}

    Emulator m_emulator;
    Debugger m_debugger;
    Overlays m_overlays;
};

int main(int argc, char** argv) {
    auto client = std::make_unique<Vectrexy>();
    auto engine = std::make_unique<SDLEngine>();
    engine->RegisterClient(*client);
    bool result = engine->Run(argc, argv);
    return result ? 0 : -1;
}
