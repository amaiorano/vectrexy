#include "Base.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include "ConsoleOutput.h"
#include "Cpu.h"
#include "Debugger.h"
#include "EngineClient.h"
#include "FileSystemUtil.h"
#include "IllegalMemoryDevice.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Overlays.h"
#include "Platform.h"
#include "Ram.h"
#include "SDLEngine.h"
#include "SyncProtocol.h"
#include "UnmappedMemoryDevice.h"
#include "Via.h"
#include <memory>
#include <random>

class Vectrexy final : public IEngineClient {
private:
    bool Init(int argc, char** argv) override {
        m_overlays.LoadOverlays();

        std::string rom = "";

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-server") {
                m_syncProtocol.InitServer();
            } else if (arg == "-client") {
                m_syncProtocol.InitClient();
            } else {
                rom = arg;
            }
        }

        // std::string rom = argc == 2 ? argv[1] : "";

        m_cpu.Init(m_memoryBus);
        m_via.Init(m_memoryBus);
        m_ram.Init(m_memoryBus);
        m_biosRom.Init(m_memoryBus);
        m_unmapped.Init(m_memoryBus);
        m_illegal.Init(m_memoryBus);
        m_cartridge.Init(m_memoryBus);
        m_debugger.Init(m_memoryBus, m_cpu, m_via);

        m_biosRom.LoadBiosRom("bios_rom.bin");

        if (!rom.empty()) {
            LoadRom(rom.c_str());
        } else {
            if (auto overlayPath = m_overlays.FindOverlay("Minestorm")) {
                ResetOverlay(overlayPath->string().c_str());
            }
        }

        Reset();

        return true;
    }

    void Reset() {
        m_cpu.Reset();
        m_via.Reset();
        m_ram.Reset();
        m_debugger.Reset();

        // Some games rely on initial random state of memory (e.g. Mine Storm)
        if (!m_syncProtocol.IsServer() && !m_syncProtocol.IsClient()) {
            const unsigned int seed = std::random_device{}();
            m_ram.Randomize(seed);
        }
    }

    bool LoadRom(const char* file) {
        if (!m_cartridge.LoadRom(file)) {
            Errorf("Failed to load rom file: %s\n", file);
            return false;
        }

        //@TODO: Show game name in title bar

        auto overlayPath = m_overlays.FindOverlay(file);
        if (overlayPath) {
            auto path = overlayPath->string();
            Errorf("Found overlay for %s: %s\n", file, path.c_str());
            ResetOverlay(path.c_str());
        } else {
            Errorf("No overlay found for %s\n", file);
            ResetOverlay();
        }

        return true;
    }

    bool Update(double frameTime, const Input& inputArg, const EmuEvents& emuEvents,
                RenderContext& renderContext) override {
        Input input = inputArg;

        if (m_syncProtocol.IsServer()) {
            m_syncProtocol.Server_SendFrameStart(frameTime, input);
        } else if (m_syncProtocol.IsClient()) {
            m_syncProtocol.Client_RecvFrameStart(frameTime, input);
        }

        if (find_if(emuEvents, [](auto& e) { return e.type == EmuEvent::Type::Reset; })) {
            Reset();
        }

        if (find_if(emuEvents, [](auto& e) { return e.type == EmuEvent::Type::OpenRomFile; })) {
            FileSystemUtil::ScopedSetCurrentDirectory scopedSetDir(m_lastOpenedFile);
            auto result =
                Platform::OpenFileDialog("Open Vectrex rom", "Vectrex Rom", "*.vec;*.bin");
            if (result && LoadRom(result->c_str())) {
                m_lastOpenedFile = *result;
                Reset();
            }
        }

        bool keepGoing =
            m_debugger.Update(frameTime, input, emuEvents, renderContext, m_syncProtocol);

        if (m_syncProtocol.IsServer()) {
            m_syncProtocol.Server_RecvFrameEnd();
        } else if (m_syncProtocol.IsClient()) {
            m_syncProtocol.Client_SendFrameEnd();
        }

        return keepGoing;
    }

    void Shutdown() override {}

    MemoryBus m_memoryBus;
    Cpu m_cpu;
    Via m_via;
    Ram m_ram;
    BiosRom m_biosRom;
    UnmappedMemoryDevice m_unmapped;
    IllegalMemoryDevice m_illegal;
    Cartridge m_cartridge;
    Debugger m_debugger;
    Overlays m_overlays;
    fs::path m_lastOpenedFile;
    SyncProtocol m_syncProtocol;
};

int main(int argc, char** argv) {
    auto client = std::make_unique<Vectrexy>();
    auto engine = std::make_unique<SDLEngine>();
    engine->RegisterClient(*client);
    bool result = engine->Run(argc, argv);
    return result ? 0 : -1;
}
