#pragma once

#include "MemoryBus.h"
#include "MemoryMap.h"
#include <array>

// Implementation of the 6522 Versatile Interface Adapter (VIA)
// Used to control all of the Vectrex peripherals, such as keypads, vector generator, DAC, sound
// chip, etc.

class Via : public IMemoryBusDevice {
public:
    void Init(MemoryBus& memoryBus);

private:
    uint8_t Read(uint16_t address) const override;
    void Write(uint16_t address, uint8_t value) override;

    uint8_t B;
    uint8_t A;

    uint8_t DataDirB;
    uint8_t DataDirA;

    uint8_t Timer1Low;
    uint8_t Timer1High;

    uint8_t Timer1LatchLow;
    uint8_t Timer1LatchHigh;

    uint8_t Timer2Low;
    uint8_t Timer2High;

    uint8_t Shift;

    uint8_t AuxCntl;
    uint8_t PeriphCntl;

    uint8_t InterruptFlag;
    uint8_t InterruptEnable;
};
