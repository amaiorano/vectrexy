#pragma once

#include "core/Base.h"

class CallStack;
class CpuRegisters;
class Cpu;
class MemoryBus;

namespace DebuggerUtil {
    // Call after every instruction is executed to update callStack
    void PostOpUpdateCallstack(CallStack& callStack, const CpuRegisters& preOpRegisters,
                               const Cpu& cpu, const MemoryBus& memoryBus);

    // Returns true if instruction at PC is a call instruction
    bool IsCall(uint16_t PC, const MemoryBus& memoryBus);

} // namespace DebuggerUtil
