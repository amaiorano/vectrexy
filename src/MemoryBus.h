#pragma once

#include "Base.h"
#include <tuple>
#include <functional>
#include <map>

using MemoryRange = std::pair<uint16_t, uint16_t>;

struct IMemoryBusDevice
{
	virtual uint8_t Read(uint16_t address) const = 0;
	virtual void Write(uint16_t address, uint8_t value) = 0;
};

class MemoryBus
{
public:
	void ConnectDevice(IMemoryBusDevice& device, MemoryRange range, int shadowDivisor)
	{
		const uint16_t size = range.second - range.first + 1;

		uint16_t shadowMask = std::numeric_limits<uint16_t>::max();
		if (shadowDivisor > 1)
		{
			assert(IsPowerOfTwo(size));
			shadowMask = static_cast<uint16_t>((size / shadowDivisor) - 1);
		}

		m_map.emplace(std::make_pair(range, DeviceInfo{ &device, range, shadowMask }));
	}

	uint8_t Read(uint16_t address) const
	{
		auto& info = FindDeviceInfo(address);
		return info.m_device->Read(address & info.m_shadowMask);
	}

	void Write(uint16_t address, uint8_t value)
	{
		auto& info = FindDeviceInfo(address);
		info.m_device->Write(address & info.m_shadowMask, value);
	}

private:
	struct DeviceInfo
	{
		IMemoryBusDevice* m_device;
		MemoryRange m_memoryRange;
		uint16_t m_shadowMask;
	};

	const DeviceInfo& FindDeviceInfo(uint16_t address) const
	{
		for (auto& entry : m_map)
		{
			if (address >= entry.first.first && address < entry.first.second)
				return entry.second;
		}
		assert(false && "Unmapped address");
		static DeviceInfo nullDeviceInfo{};
		return nullDeviceInfo;
	}

	//@TODO: replace with vector of DeviceInfo and just use linear searches
	std::map<MemoryRange, DeviceInfo> m_map;
};
