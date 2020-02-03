#pragma once

#include "core/Base.h"
#include "core/ErrorHandler.h"
#include <algorithm>
#include <functional>
#include <vector>

using MemoryRange = std::pair<uint16_t, uint16_t>;

struct IMemoryBusDevice {
    virtual uint8_t Read(uint16_t address) const = 0;
    virtual void Write(uint16_t address, uint8_t value) = 0;
    virtual void Sync(cycles_t cycles) { (void)cycles; }
};

enum class EnableSync { False, True };

class MemoryBus {
public:
    void ConnectDevice(IMemoryBusDevice& device, MemoryRange range, EnableSync enableSync) {
        m_devices.push_back(DeviceInfo{&device, range, enableSync == EnableSync::True});

        std::sort(m_devices.begin(), m_devices.end(),
                  [](const DeviceInfo& info1, const DeviceInfo& info2) {
                      return info1.memoryRange.first < info2.memoryRange.first;
                  });
    }

    //@TODO: Move this callback stuff out of here, perhaps in some DebuggerMemoryBus class.
    using OnReadCallback = std::function<void(uint16_t, uint8_t)>;
    using OnWriteCallback = std::function<void(uint16_t, uint8_t)>;
    void RegisterCallbacks(OnReadCallback onReadCallback, OnWriteCallback onWriteCallback) {
        m_onReadCallback = onReadCallback;
        m_onWriteCallback = onWriteCallback;
    }

    uint8_t Read(uint16_t address) const {
        auto& deviceInfo = FindDeviceInfo(address);
        SyncDevice(deviceInfo);

        uint8_t value = deviceInfo.device->Read(address);

        if (m_onReadCallback)
            m_onReadCallback(address, value);

        return value;
    }

    void Write(uint16_t address, uint8_t value) {
        if (m_onWriteCallback)
            m_onWriteCallback(address, value);

        auto& deviceInfo = FindDeviceInfo(address);
        SyncDevice(deviceInfo);

        deviceInfo.device->Write(address, value);
    }

    uint8_t ReadRaw(uint16_t address) const {
        auto& deviceInfo = FindDeviceInfo(address);
        return deviceInfo.device->Read(address);
    }

    uint16_t Read16(uint16_t address) const {
        // Big endian
        auto high = Read(address++);
        auto low = Read(address);
        return static_cast<uint16_t>(high) << 8 | static_cast<uint16_t>(low);
    }

    void AddSyncCycles(cycles_t cycles) {
        //@TODO: optimize this so we don't loop through all devices every time we add cycles
        for (auto& deviceInfo : m_devices) {
            if (deviceInfo.syncEnabled)
                deviceInfo.syncCycles += cycles;
        }
    }

    void Sync() {
        for (auto& deviceInfo : m_devices) {
            SyncDevice(deviceInfo);
        }
    }

private:
    struct DeviceInfo {
        IMemoryBusDevice* device = nullptr;
        MemoryRange memoryRange;
        bool syncEnabled = false;
        mutable cycles_t syncCycles = 0;
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

        ErrorHandler::Undefined("Unmapped address: $%02x\n", address);

        static struct NullMemoryBusDevice : IMemoryBusDevice {
            uint8_t Read(uint16_t) const override { return 0; };
            void Write(uint16_t, uint8_t) override{};
        } nullMemoryBusDevice;
        static DeviceInfo nullDeviceInfo{&nullMemoryBusDevice};
        return nullDeviceInfo;
    }

    void SyncDevice(const DeviceInfo& deviceInfo) const {
        if (deviceInfo.syncCycles > 0) {
            deviceInfo.device->Sync(deviceInfo.syncCycles);
            deviceInfo.syncCycles = 0;
        }
    }

    // Sorted by first address in range
    std::vector<DeviceInfo> m_devices;

    OnReadCallback m_onReadCallback;
    OnWriteCallback m_onWriteCallback;
};
