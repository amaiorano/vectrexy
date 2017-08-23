#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"

class UnmappedMemoryDevice : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus) { memoryBus.ConnectDevice(*this, MemoryMap::Unmapped.range); }

private:
    uint8_t Read(uint16_t address) const override {
        FAIL("Read from unmapped range at address $%04x\n", address);
        return 0;
    }
    void Write(uint16_t address, uint8_t value) override {
        FAIL("Write to unmapped range of value $%02x at address $%04x\n", value, address);
    }
};
