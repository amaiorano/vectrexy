#include "Base.h"
#include "MemoryBus.h"
#include "Cpu.h"
#include "Cartridge.h"
#include <memory>

int main()
{
	auto memoryBus = std::make_unique<MemoryBus>();
	auto cartridge = std::make_unique<Cartridge>();
	auto cpu = std::make_unique<Cpu>();

	cartridge->Init(*memoryBus);
	cpu->Init(*memoryBus);

	cartridge->LoadRom("../roms/Scramble (1982).vec");

	while (true)
		cpu->ExecuteInstruction();

	return 0;
}
