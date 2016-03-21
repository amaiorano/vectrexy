#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cassert>

// Build config defines
#if defined(_DEBUG)
#define CONFIG_DEBUG 1
#endif

// Disable warnings
#if _MSC_VER
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

template <typename T>
constexpr bool IsPowerOfTwo(T value)
{
	return (value != 0) && ((value & (value - 1)) == 0);
}

template <typename T, typename U>
T checked_static_cast(U value)
{
	assert(static_cast<U>(static_cast<T>(value)) == value && "Cast truncates value");
	return static_cast<T>(value);
}



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
		uint16_t size = range.second - range.first + 1;

		uint16_t shadowMask = std::numeric_limits<uint16_t>::max() - 1;
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


	std::map<MemoryRange, DeviceInfo> m_map;
};





#include <type_traits>

class Cpu
{
	struct ConditionCode
	{
		uint8_t Carry : 1;
		uint8_t Overflow : 1;
		uint8_t Zero : 1;
		uint8_t Negative : 1;
		uint8_t InterruptMask : 1;
		uint8_t HalfCarry : 1;
		uint8_t FastInterruptMask : 1;
		uint8_t Entire : 1;
	};

	struct Registers
	{
		union
		{
			struct
			{
				uint8_t A;
				uint8_t B;
			};
			uint16_t D;
		};

		uint16_t PC; // program counter
		uint16_t S; // system stack pointer
		uint16_t U; // user stack pointer
		uint8_t DP; // direct page (msb of zero-page address)
		ConditionCode CC; // condition code (aka status register)
	};
	static_assert(std::is_standard_layout<Registers>::value, "Can't use offsetof");
	static_assert(offsetof(Registers, A) < offsetof(Registers, B), "Reorder union so that A is msb and B is lsb of D");
	static_assert((offsetof(Registers, D) - offsetof(Registers, A)) == 0, "Reorder union so that A is msb and B is lsb of D");

	Registers m_reg;
};


class Cartridge : IMemoryBusDevice
{
public:
	void Init(MemoryBus& memoryBus)
	{
		memoryBus.ConnectDevice(*this, std::make_pair(0x0000, 0x7FFF), 1);
	}

private:
	uint8_t Read(uint16_t /*address*/) const override
	{
		return 0;
	}
	void Write(uint16_t /*address*/, uint8_t /*value*/) override
	{
	}
};

int main()
{
	MemoryBus memoryBus;
	Cartridge cartridge;
	//Cpu cpu;

	cartridge.Init(memoryBus);

	return 0;
}
