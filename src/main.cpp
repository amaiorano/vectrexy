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
    std::string rom = argc == 2 ? argv[1] : "";

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

    if (!rom.empty())
        cartridge->LoadRom(rom.c_str());

    cpu->Reset();

    // For now (and maybe forever), we always run via the debugger
    debugger->Run();

    return 0;
}
