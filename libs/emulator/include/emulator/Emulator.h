#pragma once

#include "core/Base.h"
#include "emulator/BiosRom.h"
#include "emulator/Cartridge.h"
#include "emulator/Cpu.h"
#include "emulator/IllegalMemoryDevice.h"
#include "emulator/Ram.h"
#include "emulator/Via.h"

class Emulator {
public:
    void Init(const char* biosRomFile);
    void Reset();
    bool LoadBios(const char* file);
    bool LoadRom(const char* file);

    cycles_t ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                AudioContext& audioContext);

    void FrameUpdate(double frameTime);

    MemoryBus& GetMemoryBus() { return m_memoryBus; }
    Cpu& GetCpu() { return m_cpu; }
    Ram& GetRam() { return m_ram; }

private:
    MemoryBus m_memoryBus;
    Cpu m_cpu;
    Via m_via;
    Ram m_ram;
    BiosRom m_biosRom;
    IllegalMemoryDevice m_illegal;
    Cartridge m_cartridge;
};
