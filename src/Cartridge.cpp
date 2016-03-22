#include "Cartridge.h"
#include "Stream.h"
#include "MemoryMap.h"
#include <vector>

namespace
{
	std::vector<uint8_t> ReadStreamUntil(FileStream& fs, uint8_t delim)
	{
		std::vector<uint8_t> result;
		uint8_t value;
		while (fs.ReadValue(value) && value != delim)
			result.push_back(value);
		if (value != delim)
			FAIL("Failed to find delim");
		return result;
	}

	std::vector<uint8_t> ReadStreamUntilEnd(FileStream& fs)
	{
		std::vector<uint8_t> result;
		uint8_t value;
		while (fs.ReadValue(value))
			result.push_back(value);
		return result;
	}
}

void Cartridge::Init(MemoryBus & memoryBus)
{
	memoryBus.ConnectDevice(*this, MemoryMap::Cartridge, 1);
}

void Cartridge::LoadRom(const char * file)
{
	FileStream fs(file, "rb");

	std::string requiredCopyright = "g GCE";
	auto copyright = ReadStreamUntil(fs, 0x80);
	if (copyright.size() < requiredCopyright.size() || memcmp(copyright.data(), requiredCopyright.data(), sizeof(requiredCopyright) != 0))
		FAIL("Invalid ROM");

	// Location of music from ROM
	//@TODO: Byte-swap on little endian!
	uint16_t musicLocation;
	fs.ReadValue(musicLocation);

	uint8_t height, width, relY, relX;
	fs.ReadValue(height);
	fs.ReadValue(width);
	fs.ReadValue(relY);
	fs.ReadValue(relX);

	auto titleBytes = ReadStreamUntil(fs, 0x80);
	std::string title(titleBytes.begin(), titleBytes.end());

	uint8_t headerEnd;
	fs.ReadValue(headerEnd);
	if (headerEnd != 0)
		FAIL("Invalid ROM");

	m_data = ReadStreamUntilEnd(fs);
}

uint8_t Cartridge::Read(uint16_t address) const
{
	assert(IsInRange(address, MemoryMap::Cartridge));
	return m_data[address - MemoryMap::Cartridge.first];
}

void Cartridge::Write(uint16_t address, uint8_t value)
{
	assert(IsInRange(address, MemoryMap::Cartridge));
	m_data[address - MemoryMap::Cartridge.first] = value;
}
