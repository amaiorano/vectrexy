#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include <array>
#include <random>

class Ram : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus) {
        memoryBus.ConnectDevice(*this, MemoryMap::Ram.range, EnableSync::False);
    }

    void Zero() { std::fill(m_data.begin(), m_data.end(), static_cast<uint8_t>(0)); }

    void Randomize(unsigned int seed) {
        std::uniform_int_distribution<> distribution{0, 255};
        std::default_random_engine engine;
        engine.seed(seed);
        std::transform(m_data.begin(), m_data.end(), m_data.begin(),
                       [&](auto&) { return static_cast<uint8_t>(distribution(engine)); });
    }

private:
    uint8_t Read(uint16_t address) const override {
        return m_data[MemoryMap::Ram.MapAddress(address)];
    }

    void Write(uint16_t address, uint8_t value) override {
        m_data[MemoryMap::Ram.MapAddress(address)] = value;
    }

    std::array<uint8_t, 1024> m_data{};
};
