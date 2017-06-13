#include "Via.h"
#include "MemoryMap.h"

void Via::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Via.range);
}

uint8_t Via::Read(uint16_t /*address*/) const {
    return uint8_t();
}

void Via::Write(uint16_t /*address*/, uint8_t /*value*/) {}
