#pragma once
#include <cstdint>

namespace Encode {
    inline uint32_t Crc32(uint32_t crc, const void* buffer, size_t len) {
        // CRC-32C (iSCSI) polynomial in reversed bit order.
        const auto POLY = 0x82f63b78;
        // CRC-32 (Ethernet, ZIP, etc.) polynomial in reversed bit order.
        // const auto POLY = 0xedb88320;

        auto buf = reinterpret_cast<const uint8_t*>(buffer);

        crc = ~crc;
        while (len--) {
            crc ^= *buf++;
            for (int k = 0; k < 8; k++)
                crc = crc & 1 ? (crc >> 1) ^ POLY : crc >> 1;
        }
        return ~crc;
    }

    template <typename T>
    uint32_t Crc32(uint32_t crc, const T& value) {
        return Crc32(crc, &value, sizeof(value));
    }
} // namespace Encode
