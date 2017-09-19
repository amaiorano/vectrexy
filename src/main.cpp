#include "Base.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include "Cpu.h"
#include "Debugger.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Platform.h"
#include "Ram.h"
#include "SDLEngine.h"
#include "UnmappedMemoryDevice.h"
#include "Via.h"
#include <memory>

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

        m_biosRom.LoadBiosRom("bios_rom.bin");

        if (!rom.empty())
            m_cartridge.LoadRom(rom.c_str());

        m_cpu.Reset();

        return true;
    }

    bool Update(double deltaTime) override {
        if (!m_debugger.Update(deltaTime))
            return false;

        m_elapsed += deltaTime;
        return true;
    }

    void Render(Display& display) override {
        //@HACK: clear lines and screen at approximately 50hz
        if (m_elapsed >= (1.0 / 50.0)) {
            m_elapsed = 0;
            m_via.m_lines.clear();
            display.Clear();
        }

        for (const auto& line : m_via.m_lines) {
            display.DrawLine(line.p0.x, line.p0.y, line.p1.x, line.p1.y);
        }
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
    double m_elapsed = 0;
};

int main(int argc, char** argv) {
    auto client = std::make_unique<Vectrexy>();
    auto engine = std::make_unique<SDLEngine>();
    engine->RegisterClient(*client);
    bool result = engine->Run(argc, argv);
    return result ? 0 : -1;
}
