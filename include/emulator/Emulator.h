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
    bool LoadRom(const char* file);

    //@TODO: Move execute instruction and updating Via out of debugger to here
    void FrameUpdate(double frameTime);

    MemoryBus& GetMemoryBus() { return m_memoryBus; }
    Cpu& GetCpu() { return m_cpu; }
    Via& GetVia() { return m_via; }
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
