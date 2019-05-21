#include "emulator/Emulator.h"

void Emulator::Init(const char* biosRomFile) {
    m_cpu.Init(m_memoryBus);
    m_via.Init(m_memoryBus);
    m_ram.Init(m_memoryBus);
    m_biosRom.Init(m_memoryBus);
    m_illegal.Init(m_memoryBus);
    m_cartridge.Init(m_memoryBus);

    m_biosRom.LoadBiosRom(biosRomFile);
}

void Emulator::Reset() {
    // Some games rely on initial random state of memory (e.g. Mine Storm)
    const unsigned int seed = std::random_device{}();
    m_ram.Randomize(seed);

    m_cpu.Reset();
    m_via.Reset();
}

bool Emulator::LoadRom(const char* file) {
    return m_cartridge.LoadRom(file);
}

void Emulator::FrameUpdate(double frameTime) {
    m_via.FrameUpdate(frameTime);
}
