#include "emulator/Cartridge.h"
#include "core/ConsoleOutput.h"
#include "core/ErrorHandler.h"
#include "core/Stream.h"
#include "emulator/MemoryMap.h"
#include <cstring>
#include <vector>

namespace {
    std::vector<uint8_t> ReadStreamUntil(FileStream& fs, uint8_t delim) {
        std::vector<uint8_t> result;
        uint8_t value;
        while (fs.ReadValue(value) && value != delim)
            result.push_back(value);
        if (value != delim)
            FAIL_MSG("Failed to find delim");
        return result;
    }

    std::vector<uint8_t> ReadStreamUntilEnd(FileStream& fs) {
        std::vector<uint8_t> result;
        uint8_t value;
        while (fs.ReadValue(value))
            result.push_back(value);
        return result;
    }

    bool IsValidRom(const char* file) {
        bool valid = false;

        try {
            FileStream fs(file, "rb");

            std::string requiredCopyright = "g GCE";
            auto copyright = ReadStreamUntil(fs, 0x80);
            if (copyright.size() < requiredCopyright.size() ||
                memcmp(copyright.data(), requiredCopyright.data(), requiredCopyright.size()) != 0) {
                Errorf("Warning: missing \"g GCE\" copyright string at start of rom\n");
            }

            // Location of music from ROM
            //@TODO: Byte-swap on little endian
            uint16_t musicLocation;
            fs.ReadValue(musicLocation);

            // Title of game is a multiline string of position, string, 0x80 as newline marker.
            const int MAX_LINES = 10; // Some reasonable max to look for
            std::string title;
            for (int line = 0; line < MAX_LINES; ++line) {
                uint8_t height, width, relY, relX;
                fs.ReadValue(height);
                // If first byte is 0, we're done
                if (height == 0) {
                    valid = true;
                    break;
                }
                fs.ReadValue(width);
                fs.ReadValue(relY);
                fs.ReadValue(relX);

                auto titleBytes = ReadStreamUntil(fs, 0x80);
                title += {titleBytes.begin(), titleBytes.end()};
                title += " ";
            }

        } catch (...) {
        }

        return valid;
    }
} // namespace

void Cartridge::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Cartridge.range, EnableSync::False);
    m_data.resize(MemoryMap::Cartridge.physicalSize, 0);
}

bool Cartridge::LoadRom(const char* file) {
    if (IsValidRom(file)) {
        FileStream fs(file, "rb");
        m_data = ReadStreamUntilEnd(fs);
        return true;
    }
    return false;
}

uint8_t Cartridge::Read(uint16_t address) const {
    auto mappedAddress = MemoryMap::Cartridge.MapAddress(address);
    if (mappedAddress >= m_data.size()) {
        ErrorHandler::Undefined("Invalid Cartridge read at $%04x\n", address);
        // Some roms erroneously access cartridge space when trying to draw vector lists (e.g. Mine
        // Storm, Polar Rescue), so by returning $01 here, we help to hide/fix these bugs. Real
        // hardware unlikely returns 0 anyway, so this may be more correct anyway?
        return 1;
    }
    return m_data[mappedAddress];
}

void Cartridge::Write(uint16_t /*address*/, uint8_t /*value*/) {
    ErrorHandler::Undefined("Writes to Cartridge ROM not allowed\n");
}
