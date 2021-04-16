#include "debugger/DebuggerUtil.h"
#include "debugger/CallStack.h"
#include "emulator/Cpu.h"
#include "emulator/MemoryBus.h"

#include <optional>

namespace {
    // If the opcode at preOpPC is a call instruction, returns the call's return address.
    // Function is expected to be called after the instruction has executed, thus preOpPC
    // is the PC before it was executed, and preOpPC != cpu.Registers().PC.
    std::optional<uint16_t> GetCallOpReturnAddress(uint16_t preOpPC, const Cpu& cpu,
                                                   const MemoryBus& memoryBus) {
        uint8_t opCode = memoryBus.ReadRaw(preOpPC);

        // If it's a call, read the return address off the stack
        switch (opCode) {
        case 0x17: // LBSR (page 0)
        case 0x8D: // BSR (page 0)
        case 0x9D: // JSR (page 0)
        case 0xAD: // JSR (page 0)
        case 0xBD: // JSR (page 0)
            // Branch and Jump push only the return address on the stack
            return memoryBus.Read16(cpu.Registers().S);

        case 0x3F: // SWI (page 0)
            // SWI pushes all registers first, starting with PC
            return memoryBus.Read16(cpu.Registers().S + 10);

        case 0x10: // Page 1
        case 0x11: // Page 2
            // Read page 1/2 op code
            opCode = memoryBus.ReadRaw(preOpPC + 1);
            switch (opCode) {
            case 0x3F: // SWI2 (page 1) or SWI3 (page 2)
                return memoryBus.Read16(cpu.Registers().S + 10);
            }

        default:
            break;
        }

        return {};
    }
} // namespace

namespace DebuggerUtil {

    void PostOpUpdateCallstack(CallStack& callStack, const CpuRegisters& preOpRegisters,
                               const Cpu& cpu, const MemoryBus& memoryBus) {
        const uint16_t preOpPC = preOpRegisters.PC;

        // Push initial frame on the first instruction
        if (callStack.Empty()) {
            callStack.Push(StackFrame{0, preOpPC, static_cast<uint16_t>(0), preOpRegisters.S});
            return;
        }

        const uint16_t currOpPC = cpu.Registers().PC;

        // Push calls
        if (auto returnAddress = GetCallOpReturnAddress(preOpPC, cpu, memoryBus)) {
            callStack.Push(StackFrame{preOpPC, currOpPC, *returnAddress, preOpRegisters.S});
        }
        // Pop returns
        else if (callStack.IsLastReturnAddress(currOpPC)) {
            // Normal return case
            callStack.Pop();

        }
        // Pop abnormal returns
        else {
            // Abnormal return: one or more return addresses were popped off the stack and
            // discarded. We must detect this, and pop off our stack frames. For example, Bedlam
            // does this.
            auto isReturnAddressRemovedFromStack = [&] {
                if (auto topS = callStack.LastStackPointer())
                    return topS > 0 && cpu.Registers().S >= topS;
                return false;
            };
            // Loop in case multiple return addresses were popped in one instruction (e.g. PULS
            // A,B,X,Y)
            while (isReturnAddressRemovedFromStack()) {
                Printf("Detected abnormal stack frame exit at PC=$%04x: %s\n", preOpPC,
                       callStack.Top()->ToString().c_str());
                callStack.Pop();
            }
        }
    }

    bool IsCall(uint16_t PC, const MemoryBus& memoryBus) {
        uint8_t opCode = memoryBus.ReadRaw(PC);

        switch (opCode) {
        case 0x17: // LBSR (page 0)
        case 0x8D: // BSR (page 0)
        case 0x9D: // JSR (page 0)
        case 0xAD: // JSR (page 0)
        case 0xBD: // JSR (page 0)
        case 0x3F: // SWI (page 0)
            return true;

        case 0x10: // Page 1
        case 0x11: // Page 2
            // Read page 1/2 op code
            opCode = memoryBus.ReadRaw(PC + 1);
            switch (opCode) {
            case 0x3F: // SWI2 (page 1) or SWI3 (page 2)
                return true;
            }

        default:
            break;
        }

        return false;
    }

} // namespace DebuggerUtil
