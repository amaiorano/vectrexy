#pragma once

// Bit operations

// Set bits in mask to 1
template <typename T, typename U>
inline void SetBits1(T& target, U mask) {
    target |= mask;
}

// Set bits in mask to 0
template <typename T, typename U>
inline void SetBits0(T& target, U mask) {
    target &= ~mask;
}

// Set bits in mask to 0 or 1 based on value of 'enable'
template <typename T, typename U>
inline void SetBits(T& target, U mask, bool enable) {
    if (enable)
        SetBits1(target, mask);
    else
        SetBits0(target, mask);
}

// Returns target with mask applied to it
template <typename T, typename U>
inline T ReadBits(T target, U mask) {
    return target & mask;
}

// Returns target with mask applied to it and shifted
template <typename T, typename U>
inline T ReadBitsWithShift(T target, U mask, U shift) {
    return (target & mask) >> shift;
}

// Returns true if any of the masked bits are 1, false otherwise
template <typename T, typename U>
inline bool TestBits(T target, U mask) {
    return ReadBits(target, mask) != 0;
}

// Returns 1 if any of the masked bits are 1, 0 otherwise
template <typename T, typename U>
inline T TestBits01(T target, U mask) {
    return ReadBits(target, mask) != 0 ? 1 : 0;
}
