#pragma once

#include "MemoryBus.h"

// Implementation of the 6522 Versatile Interface Adapter (VIA)

class Via : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;
};
