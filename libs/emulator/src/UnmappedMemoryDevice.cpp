#include "emulator/UnmappedMemoryDevice.h"
#include "core/ConsoleOutput.h"
#include "core/ErrorHandler.h"
#include "emulator/MemoryMap.h"

void UnmappedMemoryDevice::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Unmapped.range, EnableSync::False);
}

uint8_t UnmappedMemoryDevice::Read(uint16_t address) const {
    ErrorHandler::Undefined("Read from unmapped range at address $%04x\n", address);
    return 0;
}

void UnmappedMemoryDevice::Write(uint16_t address, uint8_t value) {
    ErrorHandler::Undefined("Write to unmappped range of value $%02x at address $%04x\n", value,
                            address);
}
