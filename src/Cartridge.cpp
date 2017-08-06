#include "Cartridge.h"
#include "MemoryMap.h"
#include "Stream.h"
#include <vector>

namespace {
    std::vector<uint8_t> ReadStreamUntil(FileStream& fs, uint8_t delim) {
        std::vector<uint8_t> result;
        uint8_t value;
        while (fs.ReadValue(value) && value != delim)
            result.push_back(value);
        if (value != delim)
            FAIL("Failed to find delim");
        return result;
    }

    std::vector<uint8_t> ReadStreamUntilEnd(FileStream& fs) {
        std::vector<uint8_t> result;
        uint8_t value;
        while (fs.ReadValue(value))
            result.push_back(value);
        return result;
    }
} // namespace

void Cartridge::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Cartridge.range);
    m_data.resize(MemoryMap::Cartridge.physicalSize, 0);
}

void Cartridge::LoadRom(const char* file) {
    FileStream fs(file, "rb");

    std::string requiredCopyright = "g GCE";
    auto copyright = ReadStreamUntil(fs, 0x80);
    if (copyright.size() < requiredCopyright.size() ||
        memcmp(copyright.data(), requiredCopyright.data(), sizeof(requiredCopyright) != 0))
        FAIL("Invalid ROM");

    // Location of music from ROM
    //@TODO: Byte-swap on little endian!
    uint16_t musicLocation;
    fs.ReadValue(musicLocation);

    // Title of game is a multiline string of position, string, 0x80 as newline marker.
    std::string title;
    while (true) {
        uint8_t height, width, relY, relX;
        fs.ReadValue(height);
        // If first byte is 0, we're done
        if (height == 0)
            break;
        fs.ReadValue(width);
        fs.ReadValue(relY);
        fs.ReadValue(relX);

        auto titleBytes = ReadStreamUntil(fs, 0x80);
        title += {titleBytes.begin(), titleBytes.end()};
        title += " ";
    }

    m_data = ReadStreamUntilEnd(fs);
}

uint8_t Cartridge::Read(uint16_t address) const {
    return m_data.at(MemoryMap::Cartridge.MapAddress(address));
}

void Cartridge::Write(uint16_t /*address*/, uint8_t /*value*/) {
    assert(false && "Writes to ROM not allowed");
}
