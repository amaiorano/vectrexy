#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include <array>

// Implementation of the 6522 Versatile Interface Adapter (VIA)

class Via : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;

    //@TODO: For now, just r/w to a buffer. Eventually, we'll implement the registers correctly.
    std::array<uint8_t, 16> m_data;
};
