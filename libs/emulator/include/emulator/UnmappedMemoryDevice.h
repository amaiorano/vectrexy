#pragma once

#include "MemoryBus.h"

class UnmappedMemoryDevice : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;
};
