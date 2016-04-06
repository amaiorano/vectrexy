#include "Base.h"
#include "MemoryBus.h"
#include "MemoryMap.h"
#include "Cpu.h"
#include "Ram.h"
#include "BiosRom.h"
#include "Cartridge.h"
#include <memory>

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("Usage: vectrexy <rom>\n");
		return 1;
	}

	auto memoryBus = std::make_unique<MemoryBus>();
	auto cpu = std::make_unique<Cpu>();
	auto ram = std::make_unique<Ram>();
	auto biosRom = std::make_unique<BiosRom>();
	auto cartridge = std::make_unique<Cartridge>();

	cpu->Init(*memoryBus);
	ram->Init(*memoryBus);
	biosRom->Init(*memoryBus);
	cartridge->Init(*memoryBus);

	biosRom->LoadBiosRom("bios_rom.bin");
	cartridge->LoadRom(argv[1]);

	// Start executing at the first instruction of the BIOS routines (at 0xF000)
	const uint16_t BiosRoutines = MemoryMap::Bios.first + 0x1000;
	cpu->Reset(BiosRoutines);

	while (true)
		cpu->ExecuteInstruction();

	return 0;
}
