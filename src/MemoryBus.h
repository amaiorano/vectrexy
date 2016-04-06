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
	//@TODO: I think when forwarding read/write to devices, we should pass the address in target device
	// space (i.e. address - info.m_memoryRange.first). From a device's perspective, this makes more
	// sense; plus they wouldn't have to subtract the memory range.first themselves. In fact, perhaps
	// devices (except CPU) should not depend on MemoryMap nor MemoryBus at all; we can initialize the bus
	// externally instead.

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
		return info.m_device->Read(ApplyShadowMask(address, info));
	}

	void Write(uint16_t address, uint8_t value)
	{
		auto& info = FindDeviceInfo(address);
		info.m_device->Write(ApplyShadowMask(address, info), value);
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

	uint16_t ApplyShadowMask(uint16_t address, const DeviceInfo& info) const
	{
		return info.m_memoryRange.first + ((address - info.m_memoryRange.first) & info.m_shadowMask);
	}

	//@TODO: replace with vector of DeviceInfo and just use linear searches
	std::map<MemoryRange, DeviceInfo> m_map;
};
