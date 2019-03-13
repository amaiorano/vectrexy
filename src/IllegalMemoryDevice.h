#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include "core/ConsoleOutput.h"
#include "core/ErrorHandler.h"

class IllegalMemoryDevice : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus) {
        memoryBus.ConnectDevice(*this, MemoryMap::Illegal.range, EnableSync::False);
    }

private:
    uint8_t Read(uint16_t address) const override {
        ErrorHandler::Undefined("Read from illegal range at address $%04x\n", address);
        return 0;
    }
    void Write(uint16_t address, uint8_t value) override {
        ErrorHandler::Undefined("Write to illegal range of value $%02x at address $%04x\n", value,
                                address);
    }
};
