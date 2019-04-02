#pragma once

// This header mainly contains functions needed by both Cpu and Debugger

#include "core/Base.h"
#include <type_traits>

// Convenience cast functions
template <typename T>
constexpr int16_t S16(T v) {
    return static_cast<int16_t>(static_cast<std::make_signed_t<T>>(v));
}
template <typename T>
constexpr uint16_t U16(T v) {
    return static_cast<uint16_t>(v);
}
template <typename T>
constexpr uint32_t U32(T v) {
    return static_cast<uint32_t>(v);
}
template <typename T>
constexpr uint8_t U8(T v) {
    return static_cast<uint8_t>(v);
}
// Combine two 8-bit values into a 16-bit value
constexpr uint16_t CombineToU16(uint8_t msb, uint8_t lsb) {
    return U16(msb) << 8 | U16(lsb);
}
constexpr int16_t CombineToS16(uint8_t msb, uint8_t lsb) {
    return static_cast<int16_t>(CombineToU16(msb, lsb));
}
