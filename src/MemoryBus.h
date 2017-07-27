#pragma once

#include "Base.h"
#include <algorithm>
#include <vector>

using MemoryRange = std::pair<uint16_t, uint16_t>;

struct IMemoryBusDevice {
    virtual uint8_t Read(uint16_t address) const = 0;
    virtual void Write(uint16_t address, uint8_t value) = 0;
};

class MemoryBus {
public:
    void ConnectDevice(IMemoryBusDevice& device, MemoryRange range) {
        m_devices.push_back(DeviceInfo{&device, range});

        std::sort(m_devices.begin(), m_devices.end(),
                  [](const DeviceInfo& info1, const DeviceInfo& info2) {
                      return info1.memoryRange.first < info2.memoryRange.first;
                  });
    }

    uint8_t Read(uint16_t address) const { return FindDeviceInfo(address).device->Read(address); }

    void Write(uint16_t address, uint8_t value) {
        FindDeviceInfo(address).device->Write(address, value);
    }

private:
    struct DeviceInfo {
        IMemoryBusDevice* device = nullptr;
        MemoryRange memoryRange;
    };

    const DeviceInfo& FindDeviceInfo(uint16_t address) const {
        // We assume at least 1 device is connected. This one condition allows us to check the
        // address against the end of each range in the inner loop.
        if (address >= m_devices[0].memoryRange.first) {
            for (const auto& info : m_devices) {
                if (address <= info.memoryRange.second) {
                    return info;
                }
            }
        }

        assert(false && "Unmapped address");
        static DeviceInfo nullDeviceInfo{};
        return nullDeviceInfo;
    }

    // Sorted by first address in range
    std::vector<DeviceInfo> m_devices;
};
