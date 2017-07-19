#pragma once

#include "Base.h"
#include <map>

using MemoryRange = std::pair<uint16_t, uint16_t>;

struct IMemoryBusDevice {
    virtual uint8_t Read(uint16_t address) const = 0;
    virtual void Write(uint16_t address, uint8_t value) = 0;
};

class MemoryBus {
public:
    void ConnectDevice(IMemoryBusDevice& device, MemoryRange range) {
        m_map.emplace(std::make_pair(range, DeviceInfo{&device, range}));
    }

    uint8_t Read(uint16_t address) const { return FindDeviceInfo(address).device->Read(address); }

    void Write(uint16_t address, uint8_t value) {
        FindDeviceInfo(address).device->Write(address, value);
    }

private:
    struct DeviceInfo {
        IMemoryBusDevice* device;
        MemoryRange memoryRange;
    };

    const DeviceInfo& FindDeviceInfo(uint16_t address) const {
        for (auto& entry : m_map) {
            if (address >= entry.first.first && address <= entry.first.second)
                return entry.second;
        }
        assert(false && "Unmapped address");
        static DeviceInfo nullDeviceInfo{};
        return nullDeviceInfo;
    }

    //@TODO: replace with vector of DeviceInfo and just use linear searches
    std::map<MemoryRange, DeviceInfo> m_map;
};
