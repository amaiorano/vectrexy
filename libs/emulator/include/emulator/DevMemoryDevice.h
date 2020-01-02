#pragma once

#include "MemoryBus.h"
#include <array>

// Replaces the UnmappedMemoryDevice, exposing new memory-mapped registers useful for Vectrex game
// development purposes.
class DevMemoryDevice : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;

    bool HandleDevWrite(uint16_t address, uint8_t value);

    MemoryBus* m_memoryBus{};
    uint8_t m_opFirstByte{};

    struct PrintfData {
        int argIndex = 0;
        std::array<uint8_t, 1024> args{};
        std::vector<std::string> strings{};

        PrintfData() {
            // We store addresses of elements in this array, so make sure it doesn't ever relocate.
            const size_t MaxStrings = 1024;
            strings.reserve(MaxStrings);
        }

        void Reset() {
            argIndex = 0;
            args = {};
            strings.clear();
        }
    } m_printfData;
};
