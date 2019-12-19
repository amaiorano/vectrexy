#include "emulator/Emulator.h"

void Emulator::Init(const char* biosRomFile) {
    // TODO: config option
    const bool DeveloperMode = true;

    m_cpu.Init(m_memoryBus);
    m_via.Init(m_memoryBus);
    m_ram.Init(m_memoryBus);
    m_biosRom.Init(m_memoryBus);
    m_illegal.Init(m_memoryBus);
    if (DeveloperMode) {
        m_dev.Init(m_memoryBus);
    } else {
        m_unmapped.Init(m_memoryBus);
    }
    m_cartridge.Init(m_memoryBus);

    LoadBios(biosRomFile);
}

void Emulator::Reset() {
    // Some games rely on initial random state of memory (e.g. Mine Storm)
    const unsigned int seed = std::random_device{}();
    m_ram.Randomize(seed);

    m_cpu.Reset();
    m_via.Reset();
}

bool Emulator::LoadBios(const char* file) {
    return m_biosRom.LoadBiosRom(file);
}

bool Emulator::LoadRom(const char* file) {
    return m_cartridge.LoadRom(file);
}

cycles_t Emulator::ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                      AudioContext& audioContext) {
    m_via.SetSyncContext(input, renderContext, audioContext);

    cycles_t cpuCycles = m_cpu.ExecuteInstruction(m_via.IrqEnabled(), m_via.FirqEnabled());

    // Sync all devices to consume any leftover CPU cycles
    m_memoryBus.Sync();

    return cpuCycles;
}

void Emulator::FrameUpdate(double frameTime) {
    m_via.FrameUpdate(frameTime);
}
