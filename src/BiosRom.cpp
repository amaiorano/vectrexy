#include "BiosRom.h"
#include "MemoryMap.h"
#include "Stream.h"

void BiosRom::Init(MemoryBus& memoryBus)
{
	memoryBus.ConnectDevice(*this, MemoryMap::Bios, 1);
}

void BiosRom::LoadBiosRom(const char* file)
{
	FileStream fs(file, "rb");
	fs.Read(&m_data[0], m_data.size());
}

uint8_t BiosRom::Read(uint16_t address) const
{
	assert(IsInRange(address, MemoryMap::Bios));
	return m_data[address - MemoryMap::Bios.first];
}

void BiosRom::Write(uint16_t /*address*/, uint8_t /*value*/)
{
	assert(false && "Writes to ROM not allowed");
}
