#include "Via.h"
#include "MemoryMap.h"

void Via::Init(MemoryBus& memoryBus) {
    memoryBus.ConnectDevice(*this, MemoryMap::Via.range);
    B = A = 0;
    DataDirB = DataDirA = 0;
    Timer1Low = Timer1High = 0;
    Timer1LatchLow = Timer1LatchHigh = 0;
    Timer2Low = Timer2High = 0;
    Shift = 0;
    AuxCntl = 0;
    PeriphCntl = 0;
    InterruptFlag = 0;
    InterruptEnable = 0;
}

uint8_t Via::Read(uint16_t address) const {
    const uint16_t index = MemoryMap::Via.MapAddress(address);
    switch (index) {
    case 0:
        return B;
    case 1:
        return A;
    case 2:
        return DataDirB;
    case 3:
        return DataDirA;
    case 4:
        return Timer1Low;
    case 5:
        return Timer1High;
    case 6:
        return Timer1LatchLow;
    case 7:
        return Timer1LatchHigh;
    case 8:
        return Timer2Low;
    case 9:
        return Timer2High;
    case 10:
        return Shift;
    case 11:
        return AuxCntl;
    case 12:
        return PeriphCntl;
    case 13:
        return InterruptFlag;
    case 14:
        return InterruptEnable;
    case 15:
        FAIL_MSG("A without handshake not implemented yet");
        break;
    }
    return 0;
}

void Via::Write(uint16_t address, uint8_t value) {
    const uint16_t index = MemoryMap::Via.MapAddress(address);
    switch (index) {
    case 0:
        B = value;
        break;
    case 1:
        A = value;
        break;
    case 2:
        DataDirB = value;
        break;
    case 3:
        DataDirA = value;
        break;
    case 4:
        Timer1Low = value;
        break;
    case 5:
        Timer1High = value;
        break;
    case 6:
        Timer1LatchLow = value;
        break;
    case 7:
        Timer1LatchHigh = value;
        break;
    case 8:
        Timer2Low = value;
        break;
    case 9:
        Timer2High = value;
        break;
    case 10:
        Shift = value;
        break;
    case 11:
        AuxCntl = value;
        break;
    case 12:
        PeriphCntl = value;
        break;
    case 13:
        InterruptFlag = value;
        break;
    case 14:
        InterruptEnable = value;
        break;
    case 15:
        FAIL_MSG("A without handshake not implemented yet");
        break;
    }
}
