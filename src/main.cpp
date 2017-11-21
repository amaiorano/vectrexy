#include "Base.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include "Cpu.h"
#include "Debugger.h"
#include "EngineClient.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
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
        std::string rom = argc == 2 ? argv[1] : "";

        m_cpu.Init(m_memoryBus);
        m_via.Init(m_memoryBus);
        m_ram.Init(m_memoryBus);
        m_biosRom.Init(m_memoryBus);
        m_unmapped.Init(m_memoryBus);
        m_cartridge.Init(m_memoryBus);
        m_debugger.Init(m_memoryBus, m_cpu, m_via);

        // Some games rely on initial random state of memory (e.g. Mine Storm)
        const unsigned int seed = std::random_device{}();
        m_ram.Randomize(seed);

        m_biosRom.LoadBiosRom("bios_rom.bin");

        if (!rom.empty())
            m_cartridge.LoadRom(rom.c_str());

        m_cpu.Reset();

        return true;
    }

    bool Update(double deltaTime, const Input& input) override {
        if (!m_debugger.Update(deltaTime, input))
            return false;

        return true;
    }

    void Render(double deltaTime, Display& display) override {
        display.DrawLines(m_via.m_lines);

        if (deltaTime > 0)
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
};

int main(int argc, char** argv) {
    auto client = std::make_unique<Vectrexy>();
    auto engine = std::make_unique<SDLEngine>();
    engine->RegisterClient(*client);
    bool result = engine->Run(argc, argv);
    return result ? 0 : -1;
}
