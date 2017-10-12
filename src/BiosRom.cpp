#include "BiosRom.h"
#include "MemoryMap.h"
#include "Stream.h"

void BiosRom::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Bios.range);
}

void BiosRom::LoadBiosRom(const char* file) {
    FileStream fs(file, "rb");
    fs.Read(&m_data[0], m_data.size());
}

uint8_t BiosRom::Read(uint16_t address) const {
    return m_data[MemoryMap::Bios.MapAddress(address)];
}

void BiosRom::Write(uint16_t /*address*/, uint8_t /*value*/) {
    FAIL_MSG("Writes to BIOS ROM not allowed");
}
