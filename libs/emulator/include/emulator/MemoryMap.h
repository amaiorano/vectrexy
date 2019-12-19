#pragma once

#include <cassert>
#include <cstdint>
#include <utility>

namespace MemoryMap {
    template <typename T1, typename T2, typename T3>
    constexpr inline bool IsInRange(T1 value, const std::pair<T2, T3>& range) {
        return value >= range.first && value <= range.second;
    }

    struct Mapping {
        std::pair<uint16_t, uint16_t> range; // [first address, last address]
        const size_t physicalSize;           // size in bytes of address range, including shadowed
        const size_t logicalSize;            // size in bytes of unshadowed address range

        constexpr Mapping(uint16_t first, uint16_t last, size_t shadowDivisor = 1)
            : range{first, last}
            , physicalSize{last - first + 1u}
            , logicalSize{(last - first + 1u) / shadowDivisor} {}

        // Maps input address to [0, range.first + logicalSize]
        //@TODO: Better name (Normalize? Wrap?)
        uint16_t MapAddress(uint16_t address) const {
            ASSERT_MSG(IsInRange(address, range),
                       "Mapping address out of range! Value: $%04x, Range: [$%04x, $%04x]", address,
                       range.first, range.second);
            return (address - range.first) % logicalSize;
        }
    };

    // Cartridge ROM space
    // Turns out the Vectrex can actually address 48K for the cartridge, although it documents the
    // first 32K for cartridge, and the next 16K as "unmapped".
    constexpr auto Cartridge = Mapping(0x0000, 0xBFFF);
    static_assert(Cartridge.physicalSize == 32768 + 16384, "");

    // Between Cartridge's 48K and RAM is an unmapped 2K area
    constexpr auto Unmapped = Mapping(0xC000, 0xC7FF);
    static_assert(Unmapped.physicalSize == 2048);

    // RAM 1 KB shadowed twice
    // NOTES:
    // - C800-C87F and CBEA-CBFE are used by BIOS for housekeeping
    // - C880-CBEA (874 bytes) can by used by programmer, which includes system stack (S)
    constexpr auto Ram = Mapping(0xC800, 0xCFFF, 2);
    static_assert(Ram.physicalSize == 2048, "");

    // 6522 VIA 16 bytes shadowed 128 times
    constexpr auto Via = Mapping(0xD000, 0xD7FF, 128);
    static_assert(Via.physicalSize == 2048, "");

    // Both VIA + RAM selected
    constexpr auto Illegal = Mapping(0xD800, 0xDFFF);
    static_assert(Illegal.physicalSize == 2048, "");

    // Mine Storm (first half: 0xE000-0xEFFF) + BIOS (second half: 0xF000-0xFFFF)
    constexpr auto Bios = Mapping(0xE000, 0xFFFF);
    static_assert(Bios.physicalSize == 8192, "");

} // namespace MemoryMap
