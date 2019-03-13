#include "BiosRom.h"
#include "MemoryMap.h"
#include "core/ErrorHandler.h"
#include "core/Stream.h"

void BiosRom::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Bios.range, EnableSync::False);
}

void BiosRom::LoadBiosRom(const char* file) {
    FileStream fs(file, "rb");
    fs.Read(&m_data[0], m_data.size());
}

uint8_t BiosRom::Read(uint16_t address) const {
    return m_data[MemoryMap::Bios.MapAddress(address)];
}

void BiosRom::Write(uint16_t /*address*/, uint8_t /*value*/) {
    ErrorHandler::Undefined("Writes to BIOS ROM not allowed\n");
}
