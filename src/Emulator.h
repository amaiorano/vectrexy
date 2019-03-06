#pragma once

#include "Base.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include "Cpu.h"
#include "IllegalMemoryDevice.h"
#include "Ram.h"
#include "Via.h"

class Emulator {
public:
    void Init();
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
