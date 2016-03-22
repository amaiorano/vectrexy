#pragma once

#include <cstdint>
#include <utility>

namespace MemoryMap
{
	// Cartridge ROM space
	constexpr auto Cartridge = std::make_pair(0x0000, 0x7FFF);
	constexpr size_t CartridgeSize = Cartridge.second - Cartridge.first + 1;
	static_assert(CartridgeSize == 32768, "");

	// Umapped
	constexpr auto Unmapped = std::make_pair(0x8000, 0xC7FF);
	constexpr size_t UnmappedSize = Unmapped.second - Unmapped.first + 1;
	static_assert(UnmappedSize == 18432, "");

	// 1 KB shadowed twice
	constexpr auto Ram = std::make_pair(0xC800, 0xCFFF);
	constexpr size_t RamSize = Ram.second - Ram.first + 1;
	static_assert(RamSize == 2048, "");

	// 6522 VIA 16 bytes shadowed 128 times
	constexpr auto Via = std::make_pair(0xD000, 0xD7FF);
	constexpr size_t ViaSize = Via.second - Via.first + 1;
	static_assert(ViaSize == 2048, "");

	// Both VIA + RAM selected
	constexpr auto Illegal = std::make_pair(0xD800, 0xDFFF);
	constexpr size_t IllegalSize = Illegal.second - Illegal.first + 1;
	static_assert(IllegalSize == 2048, "");

	// Mine Storm (first half) + BIOS (second half)
	constexpr auto Bios = std::make_pair(0xE000, 0xFFFF);
	constexpr size_t BiosSize = Bios.second - Bios.first + 1;
	static_assert(BiosSize == 8192, "");
}
