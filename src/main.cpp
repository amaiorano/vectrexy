#include "Base.h"
#include "MemoryBus.h"
#include "Cpu.h"
#include "Cartridge.h"

int main()
{
	MemoryBus memoryBus;
	Cartridge cartridge;
	//Cpu cpu;

	cartridge.Init(memoryBus);

	cartridge.LoadRom("../roms/Scramble (1982).vec");

	return 0;
}
