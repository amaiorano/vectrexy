#include "emulator/Cpu.h"
#include "emulator/CpuOpCodes.h"
#include "emulator/MemoryBus.h"
#include <array>

namespace Trace {
    struct Instruction {
        const CpuOp* cpuOp;
        int page;
        std::array<uint8_t, 5> opBytes; // Max 2 byte opcode + 3 byte operands
        size_t firstOperandIndex = 0;

        uint8_t GetOperand(size_t index) const { return opBytes[firstOperandIndex + index]; }
    };

    struct InstructionTraceInfo {
        Instruction instruction{};
        CpuRegisters preOpCpuRegisters;
        CpuRegisters postOpCpuRegisters{};
        cycles_t elapsedCycles{};

        static const size_t MaxMemoryAccesses = 16;
        struct MemoryAccess {
            uint16_t address{};
            uint16_t value{};
            bool read{};
        };
        std::array<MemoryAccess, MaxMemoryAccesses> memoryAccesses;
        size_t numMemoryAccesses = 0;

        void AddMemoryAccess(uint16_t address, uint16_t value, bool read) {
            assert(numMemoryAccesses < memoryAccesses.size());
            memoryAccesses[numMemoryAccesses++] = {address, value, read};
        }
    };

    inline Instruction ReadInstruction(uint16_t opAddr, const MemoryBus& memoryBus) {
        Instruction instruction{};

        // Always read max opBytes size even if not all the bytes are for this instruction. We can't
        // really know up front how many bytes an op will take because indexed instructions
        // sometimes read an extra operand byte (determined dynamically).
        for (auto& byte : instruction.opBytes)
            byte = memoryBus.Read(opAddr++);

        int cpuOpPage = 0;
        size_t opCodeIndex = 0;
        if (IsOpCodePage1(instruction.opBytes[opCodeIndex])) {
            cpuOpPage = 1;
            ++opCodeIndex;
        } else if (IsOpCodePage2(instruction.opBytes[opCodeIndex])) {
            cpuOpPage = 2;
            ++opCodeIndex;
        }

        instruction.cpuOp = &LookupCpuOpRuntime(cpuOpPage, instruction.opBytes[opCodeIndex]);
        instruction.page = cpuOpPage;
        instruction.firstOperandIndex = opCodeIndex + 1;
        return instruction;
    }

    inline void PreOpWriteTraceInfo(InstructionTraceInfo& traceInfo,
                                    const CpuRegisters& cpuRegisters,
                                    /*const*/ MemoryBus& memoryBus) {
        memoryBus.SetCallbacksEnabled(false);
        traceInfo.instruction = ReadInstruction(cpuRegisters.PC, memoryBus);
        traceInfo.preOpCpuRegisters = cpuRegisters;
        memoryBus.SetCallbacksEnabled(true);
    }

    inline void PostOpWriteTraceInfo(InstructionTraceInfo& traceInfo,
                                     const CpuRegisters& cpuRegisters, cycles_t elapsedCycles) {
        traceInfo.postOpCpuRegisters = cpuRegisters;
        traceInfo.elapsedCycles = elapsedCycles;
    }
} // namespace Trace
