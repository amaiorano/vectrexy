#pragma once

#include "MemoryBus.h"
#include "core/Base.h"
#include <array>

// 8K ROM chip that holds the BIOS (Mine Storm + BIOS code)
class BiosRom : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);
    bool LoadBiosRom(const char* file);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;

    std::array<uint8_t, 8 * 1024> m_data{};
};
