#include "Base.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include "Cpu.h"
#include "Debugger.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Platform.h"
#include "Ram.h"
#include "Via.h"
#include <memory>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: vectrexy <rom>\n");
        return 1;
    }

    auto memoryBus = std::make_unique<MemoryBus>();
    auto cpu = std::make_unique<Cpu>();
    auto via = std::make_unique<Via>();
    auto ram = std::make_unique<Ram>();
    auto biosRom = std::make_unique<BiosRom>();
    auto cartridge = std::make_unique<Cartridge>();
    auto debugger = std::make_unique<Debugger>();

    cpu->Init(*memoryBus);
    via->Init(*memoryBus);
    ram->Init(*memoryBus);
    biosRom->Init(*memoryBus);
    cartridge->Init(*memoryBus);
    debugger->Init(*memoryBus, *cpu);

    biosRom->LoadBiosRom("bios_rom.bin");
    cartridge->LoadRom(argv[1]);

    // Start executing at the first instruction of the BIOS routines (at 0xF000)
    const uint16_t BiosRoutines = MemoryMap::Bios.range.first + 0x1000;
    cpu->Reset(BiosRoutines);

    // For now (and maybe forever), we always run via the debugger
    debugger->Run();

    return 0;
}
