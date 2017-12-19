#include "Base.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include "Cpu.h"
#include "Debugger.h"
#include "EngineClient.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Overlays.h"
#include "Platform.h"
#include "Ram.h"
#include "SDLEngine.h"
#include "UnmappedMemoryDevice.h"
#include "Via.h"
#include <memory>
#include <random>

class Vectrexy final : public IEngineClient {
private:
    bool Init(int argc, char** argv) override {
        m_overlays.LoadOverlays();

        std::string rom = argc == 2 ? argv[1] : "";

        m_cpu.Init(m_memoryBus);
        m_via.Init(m_memoryBus);
        m_ram.Init(m_memoryBus);
        m_biosRom.Init(m_memoryBus);
        m_unmapped.Init(m_memoryBus);
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
        const unsigned int seed = std::random_device{}();
        m_ram.Randomize(seed);
    }

    bool LoadRom(const char* file) {
        if (!m_cartridge.LoadRom(file)) {
            fprintf(stderr, "Failed to load rom file: %s\n", file);
            return false;
        }

        //@TODO: Show game name in title bar

        auto overlayPath = m_overlays.FindOverlay(file);
        if (overlayPath) {
            auto path = overlayPath->string().c_str();
            fprintf(stderr, "Found overlay for %s: %s\n", file, path);
            ResetOverlay(path);
        } else {
            fprintf(stderr, "No overlay found for %s\n", file);
            ResetOverlay();
        }

        return true;
    }

    bool Update(double frameTime, const Input& input, const EmuEvents& emuEvents) override {

        if (find_if(emuEvents, [](auto& e) { return e.type == EmuEvent::Type::Reset; })) {
            Reset();
        }

        if (find_if(emuEvents, [](auto& e) { return e.type == EmuEvent::Type::OpenRomFile; })) {
            auto result =
                Platform::OpenFileDialog("Open Vectrex rom", "Vectrex Rom", "*.vec;*.bin");
            if (result && LoadRom(result->c_str())) {
                Reset();
            }
        }

        if (!m_debugger.Update(frameTime, input, emuEvents))
            return false;

        return true;
    }

    void Render(double frameTime, Display& display) override {
        display.DrawLines(m_via.m_lines);

        if (frameTime > 0)
            m_via.m_lines.clear();
    }

    void Shutdown() override {}

    MemoryBus m_memoryBus;
    Cpu m_cpu;
    Via m_via;
    Ram m_ram;
    BiosRom m_biosRom;
    UnmappedMemoryDevice m_unmapped;
    Cartridge m_cartridge;
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
