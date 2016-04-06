#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include <array>

class Ram : public IMemoryBusDevice
{
public:
	void Init(MemoryBus& memoryBus)
	{
		memoryBus.ConnectDevice(*this, MemoryMap::Ram, 2);
	}
	uint8_t Read(uint16_t address) const override
	{
		return m_data[address - MemoryMap::Ram.first];
	}
	void Write(uint16_t address, uint8_t value) override
	{
		m_data[address - MemoryMap::Ram.first] = value;
	}

private:
	std::array<uint8_t, 1024> m_data;
};
