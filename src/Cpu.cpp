#include "Cpu.h"
#include "CpuHelpers.h"
#include "CpuOpCodes.h"
#include "MemoryBus.h"
#include <array>
#include <type_traits>

namespace {
    template <typename T>
    size_t NumBitsSet(T value) {
        size_t count = 0;
        while (value) {
            if ((value & 0x1) != 0)
                ++count;
            value >>= 1;
        }
        return count;
    }
} // namespace

class CpuImpl : public CpuRegisters {
public:
    MemoryBus* m_memoryBus = nullptr;
    cycles_t m_cycles = 0;

    void Init(MemoryBus& memoryBus) {
        m_memoryBus = &memoryBus;
        Reset(0);
    }

    void Reset(uint16_t initialPC) {
        X = 0;
        Y = 0;
        U = 0;
        S = 0; // BIOS will init this to 0xCBEA, which is the last byte of programmer-usable RAM
        PC = initialPC;
        DP = 0;

        CC.Value = 0;
        CC.InterruptMask = true;
        CC.FastInterruptMask = true;
    }

    uint8_t Read8(uint16_t address) { return m_memoryBus->Read(address); }

    uint16_t Read16(uint16_t address) {
        // Big endian
        auto high = m_memoryBus->Read(address++);
        auto low = m_memoryBus->Read(address);
        return CombineToU16(high, low);
    }

    uint8_t ReadPC8() { return Read8(PC++); }
    uint16_t ReadPC16() {
        uint16_t value = Read16(PC);
        PC += 2;
        return value;
    }

    void Push8(uint16_t& stackPointer, uint8_t value) { m_memoryBus->Write(--stackPointer, value); }

    uint8_t Pop8(uint16_t& stackPointer) {
        auto value = m_memoryBus->Read(stackPointer++);
        return value;
    }

    void Push16(uint16_t& stackPointer, uint16_t value) {
        m_memoryBus->Write(--stackPointer, U8(value & 0xFF)); // Low
        m_memoryBus->Write(--stackPointer, U8(value >> 8));   // High
    }

    uint16_t Pop16(uint16_t& stackPointer) {
        auto high = m_memoryBus->Read(stackPointer++);
        auto low = m_memoryBus->Read(stackPointer++);
        return CombineToU16(high, low);
    }

    uint16_t ReadDirectEA() {
        // EA = DP : (PC)
        uint16_t EA = CombineToU16(DP, ReadPC8());
        return EA;
    }

    uint16_t ReadIndexedEA() {
        // In all indexed addressing one of the pointer registers (X, Y, U, S and sometimes PC) is
        // used in a calculation of the EA. The postbyte specifies type and variation of addressing
        // mode as well as pointer registers to be used.
        //@TODO: add extra cycles

        auto RegisterSelect = [this](uint8_t postbyte) -> uint16_t& {
            switch ((postbyte >> 5) & 0b11) {
            case 0b00:
                return X;
            case 0b01:
                return Y;
            case 0b10:
                return U;
            default: // 0b11:
                return S;
            }
        };

        uint16_t EA = 0;
        uint8_t postbyte = ReadPC8();
        bool supportsIndirect = true;
        int extraCycles = 0;

        if ((postbyte & BITS(7)) == 0) // (+/- 4 bit offset),R
        {
            // postbyte is a 5 bit two's complement number we convert to 8 bit.
            // So if bit 4 is set (sign bit), we extend the sign bit by turning on bits 6,7,8;
            uint8_t offset = postbyte & 0b0000'1111;
            if (postbyte & BITS(4))
                offset |= 0b1110'0000;
            EA = RegisterSelect(postbyte) + S16(offset);
            supportsIndirect = false;
            extraCycles = 1;
        } else {
            switch (postbyte & 0b1111) {
            case 0b0000: { // ,R+
                auto& reg = RegisterSelect(postbyte);
                EA = reg;
                reg += 1;
                supportsIndirect = false;
                extraCycles = 2;
            } break;
            case 0b0001: { // ,R++
                auto& reg = RegisterSelect(postbyte);
                EA = reg;
                reg += 2;
                extraCycles = 3;
            } break;
            case 0b0010: { // ,-R
                auto& reg = RegisterSelect(postbyte);
                reg -= 1;
                EA = reg;
                supportsIndirect = false;
                extraCycles = 2;
            } break;
            case 0b0011: { // ,--R
                auto& reg = RegisterSelect(postbyte);
                reg -= 2;
                EA = reg;
                extraCycles = 3;
            } break;
            case 0b0100: // ,R
                EA = RegisterSelect(postbyte);
                break;
            case 0b0101: // (+/- B),R
                EA = RegisterSelect(postbyte) + S16(B);
                extraCycles = 1;
                break;
            case 0b0110: // (+/- A),R
                EA = RegisterSelect(postbyte) + S16(A);
                extraCycles = 1;
                break;
            case 0b0111:
                FAIL("Illegal");
                break;
            case 0b1000: { // (+/- 7 bit offset),R
                uint8_t postbyte2 = ReadPC8();
                EA = RegisterSelect(postbyte) + S16(postbyte2);
                extraCycles = 1;
            } break;
            case 0b1001: { // (+/- 15 bit offset),R
                uint8_t postbyte2 = ReadPC8();
                uint8_t postbyte3 = ReadPC8();
                EA = RegisterSelect(postbyte) + CombineToS16(postbyte2, postbyte3);
                extraCycles = 4;
            } break;
            case 0b1010:
                FAIL("Illegal");
                break;
            case 0b1011: // (+/- D),R
                EA = RegisterSelect(postbyte) + S16(D);
                extraCycles = 4;
                break;
            case 0b1100: { // (+/- 7 bit offset),PC
                uint8_t postbyte2 = ReadPC8();
                EA = PC + S16(postbyte2);
                extraCycles = 1;
            } break;
            case 0b1101: { // (+/- 15 bit offset),PC
                uint8_t postbyte2 = ReadPC8();
                uint8_t postbyte3 = ReadPC8();
                EA = PC + CombineToS16(postbyte2, postbyte3);
                extraCycles = 5;
            } break;
            case 0b1110:
                FAIL("Illegal");
                break;
            case 0b1111: { // [address] (Indirect-only)
                uint8_t postbyte2 = ReadPC8();
                uint8_t postbyte3 = ReadPC8();
                EA = CombineToS16(postbyte2, postbyte3);
                extraCycles = 5;
            } break;
            default:
                FAIL("Illegal");
                break;
            }
        }

        if (supportsIndirect && (postbyte & BITS(4))) {
            uint8_t msb = m_memoryBus->Read(EA);
            uint8_t lsb = m_memoryBus->Read(EA + 1);
            EA = CombineToU16(lsb, msb);
            extraCycles += 3;
        }

        m_cycles += extraCycles;

        return EA;
    }

    uint16_t ReadExtendedEA() {
        // Contents of 2 bytes following opcode byte specify 16-bit effective address (always 3 byte
        // instruction) EA = (PC) : (PC + 1)
        auto msb = ReadPC8();
        auto lsb = ReadPC8();
        uint16_t EA = CombineToU16(msb, lsb);

        // @TODO: "As a special case of indexed addressing, one level of indirection may be added to
        // extended addressing. In extended indirect,
        //         the two bytes following the postbyte of an indexed instruction contain the
        //         address of the data."
        // *** Is this handled under Indexed??

        return EA;
    }

    // Read 16-bit effective address based on addressing mode
    template <AddressingMode addressingMode>
    uint16_t ReadEA16() {
        assert(false && "Not implemented for addressing mode");
        return 0xFFFF;
    }
    template <>
    uint16_t ReadEA16<AddressingMode::Indexed>() {
        return ReadIndexedEA();
    }
    template <>
    uint16_t ReadEA16<AddressingMode::Extended>() {
        return ReadExtendedEA();
    }
    template <>
    uint16_t ReadEA16<AddressingMode::Direct>() {
        return ReadDirectEA();
    }

    // Read CPU op's value (8/16 bit) either directly or indirectly (via EA) depending on addressing
    // mode Default template assumes operand is EA and de-refs it
    template <AddressingMode addressingMode>
    uint16_t ReadOperandValue16() {
        auto EA = ReadEA16<addressingMode>();
        return Read16(EA);
    }
    // Specialize for Immediate mode where we don't de-ref
    template <>
    uint16_t ReadOperandValue16<AddressingMode::Immediate>() {
        return ReadPC16();
    }

    template <AddressingMode addressingMode>
    uint8_t ReadOperandValue8() {
        auto EA = ReadEA16<addressingMode>();
        return Read8(EA);
    }
    template <>
    uint8_t ReadOperandValue8<AddressingMode::Immediate>() {
        return ReadPC8();
    }

    // Read CPU op's relative offset from next 8/16 bits
    int8_t ReadRelativeOffset8() { return static_cast<int8_t>(ReadPC8()); }
    int16_t ReadRelativeOffset16() { return static_cast<int16_t>(ReadPC16()); }

    template <int page, uint8_t opCode>
    void OpLD(uint8_t& targetReg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        CC.Negative = (value & BITS(7)) != 0;
        CC.Zero = (value == 0);
        CC.Overflow = 0;
        targetReg = value;
    }

    template <int page, uint8_t opCode>
    void OpLD(uint16_t& targetReg) {
        uint16_t value = ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>();
        CC.Negative = (value & BITS(15)) != 0;
        CC.Zero = (value == 0);
        CC.Overflow = 0;
        targetReg = value;
    }

    template <int page, uint8_t opCode>
    void OpST(const uint8_t& sourceReg) {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        m_memoryBus->Write(EA, sourceReg);
        CC.Negative = (sourceReg & BITS(7)) != 0;
        CC.Zero = (sourceReg == 0);
        CC.Overflow = 0;
    }

    template <int page, uint8_t opCode>
    void OpST(const uint16_t& sourceReg) {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        m_memoryBus->Write(EA, U8(sourceReg >> 8));       // High
        m_memoryBus->Write(EA + 1, U8(sourceReg & 0xFF)); // Low
        CC.Negative = (sourceReg & BITS(7)) != 0;
        CC.Zero = (sourceReg == 0);
        CC.Overflow = 0;
    }

    template <int page, uint8_t opCode>
    void OpJSR() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        Push16(S, PC);
        PC = EA;
    }

    template <int page, uint8_t opCode>
    void OpCLR() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        m_memoryBus->Write(EA, 0);
        CC.Negative = 0;
        CC.Zero = 1;
        CC.Overflow = 0;
        CC.Carry = 0;
    }

    // Implement two's complement subtract
    template <typename T>
    static T Subtract(T reg, T value, ConditionCode& CC) {
        // Instead of subtracting, we add the 2's complement of the value
        uint32_t a = U32(reg);
        uint32_t b = ~U32(value); // 1's complement (we add one next to make it 2's complement)
        uint32_t r = a + b + 1;

        CC.Carry = (r & 0xFFFF'0000) != 0;

        // For subtraction, C is the complement of the resulting carry since it represents a borrow
        CC.Carry = !CC.Carry;

        // If we look at sign bits of a, b, r, then overflow is set if 0 0 1 or 1 1 0
        CC.Overflow = ((a ^ r) & (a ^ b) & BITS(15)) != 0;

        CC.Zero = (r == 0);
        CC.Negative = (r & BITS(15)) != 0;

        return static_cast<T>(r);
    }

    // SUBA, SUBB
    template <int page, uint8_t opCode>
    void OpSUB(uint8_t& reg) {
        const uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        reg = Subtract(reg, value, CC);
    }

    // SUBD
    template <int page, uint8_t opCode>
    void OpSUB(uint16_t& reg) {
        const uint16_t value = ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>();
        reg = Subtract(reg, value, CC);
    }

    // INCA, INCB
    template <int page, uint8_t opCode>
    void OpINC(uint8_t& reg) {
        ++reg;
        CC.Overflow = reg == 0;
        CC.Zero = (reg == 0);
        CC.Negative = (reg & BITS(7)) != 0;
    }

    // INC <address>
    template <int page, uint8_t opCode>
    void OpINC() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        ++value;
        m_memoryBus->Write(EA, value);
        CC.Overflow = value == 0;
        CC.Zero = (value == 0);
        CC.Negative = (value & BITS(7)) != 0;
    }

    // DECA, DECB
    template <int page, uint8_t opCode>
    void OpDEC(uint8_t& reg) {
        --reg;
        CC.Overflow = reg == 0;
        CC.Zero = (reg == 0);
        CC.Negative = (reg & BITS(7)) != 0;
    }

    // DEC <address>
    template <int page, uint8_t opCode>
    void OpDEC() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        --value;
        m_memoryBus->Write(EA, value);
        CC.Overflow = value == 0;
        CC.Zero = (value == 0);
        CC.Negative = (value & BITS(7)) != 0;
    }

    template <int page, uint8_t opCode>
    void OpJMP() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        PC = EA;
    }

    template <int page, uint8_t opCode>
    void OpPSH(uint16_t& stackReg) {
        assert(&stackReg == &S || &stackReg == &U);
        const uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        if (value & BITS(7))
            Push16(stackReg, PC);
        if (value & BITS(6)) {
            auto otherStackReg = &stackReg == &S ? U : S;
            Push16(stackReg, otherStackReg);
        }
        if (value & BITS(5))
            Push16(stackReg, Y);
        if (value & BITS(4))
            Push16(stackReg, X);
        if (value & BITS(3))
            Push8(stackReg, DP);
        if (value & BITS(2))
            Push8(stackReg, B);
        if (value & BITS(1))
            Push8(stackReg, A);
        if (value & BITS(0))
            Push8(stackReg, CC.Value);

        m_cycles += NumBitsSet(value); // 1 cycle per value that's pushed
    }

    template <int page, uint8_t opCode>
    void OpPUL(uint16_t& stackReg) {
        assert(&stackReg == &S || &stackReg == &U);
        const uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        if (value & BITS(0))
            CC.Value = Pop8(stackReg);
        if (value & BITS(1))
            A = Pop8(stackReg);
        if (value & BITS(2))
            B = Pop8(stackReg);
        if (value & BITS(3))
            DP = Pop8(stackReg);
        if (value & BITS(4))
            X = Pop16(stackReg);
        if (value & BITS(5))
            Y = Pop16(stackReg);
        if (value & BITS(6)) {
            auto& otherStackReg = &stackReg == &S ? U : S;
            otherStackReg = Pop16(stackReg);
        }
        if (value & BITS(7))
            PC = Pop16(stackReg);

        m_cycles += NumBitsSet(value); // 1 cycle per value that's pulled
    }

    template <int page, uint8_t opCode>
    void OpTST(const uint8_t& value) {
        CC.Negative = (value & BITS(7)) != 0;
        CC.Zero = value == 0;
        CC.Overflow = 0;
    }

    template <int page, uint8_t opCode>
    void OpTST() {
        OpTST<page, opCode>(ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>());
    }

    template <int page, uint8_t opCode>
    void OpOR(uint8_t& reg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        reg = reg | value;
        // For ORCC, we don't update CC. @TODO: separate function?
        if (&reg != &CC.Value) {
            CC.Negative = (value & BITS(7)) != 0;
            CC.Zero = value == 0;
            CC.Overflow = 0;
        }
    }

    template <int page, uint8_t opCode>
    void OpCMP(const uint8_t& reg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        Subtract(reg, value, CC);
    }

    template <int page, uint8_t opCode>
    void OpCMP(const uint16_t& reg) {
        uint16_t value = ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>();
        Subtract(reg, value, CC);
    }

    template <int page, uint8_t opCode>
    void OpBIT(const uint8_t& reg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        uint8_t result = reg & value;
        CC.Negative = (result & BITS(7)) != 0;
        CC.Zero = result == 0;
        CC.Overflow = 0;
    }

    // Helper for conditional branch ops. Always reads relative offset, and if condition is true,
    // applies it to PC.
    template <typename CondFunc>
    void BranchIf(CondFunc condFunc) {
        int8_t offset = ReadRelativeOffset8();
        if (condFunc()) {
            PC += offset;
            m_cycles += 1; // Extra cycle if branch is taken
        }
    }

    cycles_t ExecuteInstruction() {
        m_cycles = 0;

        auto UnhandledOp = [this](const CpuOp& cpuOp) {
            (void)cpuOp;
            FAIL("Unhandled Op: %s", cpuOp.name);
        };

        int cpuOpPage = 0;
        uint8_t opCodeByte = ReadPC8();
        if (IsOpCodePage1(opCodeByte)) {
            cpuOpPage = 1; //@TODO: 1 cycle (see CpuOpsPage0)
            opCodeByte = ReadPC8();
        } else if (IsOpCodePage2(opCodeByte)) {
            cpuOpPage = 2; //@TODO: 1 cycle (see CpuOpsPage0)
            opCodeByte = ReadPC8();
        }

        const CpuOp& cpuOp = LookupCpuOpRuntime(cpuOpPage, opCodeByte);

        assert(cpuOp.cycles > 0 && "TODO: look at how to handle cycles for this instruction");
        //@TODO: Handle cycle counting for interrupts (SWI[2/3], [F]IRQ, NMI) and RTI
        m_cycles += cpuOp.cycles; // Base cycles for this instruction

        assert(cpuOp.addrMode != AddressingMode::Illegal && "Illegal instruction!");
        assert(cpuOp.addrMode != AddressingMode::Variant &&
               "Page 1/2 instruction, should have read next byte by now");

        switch (cpuOpPage) {
        case 0:
            switch (cpuOp.opCode) {
            case 0x9D:
                OpJSR<0, 0x9D>();
                break;
            case 0xAD:
                OpJSR<0, 0xAD>();
                break;
            case 0xBD:
                OpJSR<0, 0xBD>();
                break;

            // 8-bit LD
            case 0x86:
                OpLD<0, 0x86>(A);
                break;
            case 0x96:
                OpLD<0, 0x96>(A);
                break;
            case 0xA6:
                OpLD<0, 0xA6>(A);
                break;
            case 0xB6:
                OpLD<0, 0xB6>(A);
                break;
            case 0xC6:
                OpLD<0, 0xC6>(B);
                break;
            case 0xD6:
                OpLD<0, 0xD6>(B);
                break;
            case 0xE6:
                OpLD<0, 0xE6>(B);
                break;
            case 0xF6:
                OpLD<0, 0xF6>(B);
                break;
            // 16-bit LD
            case 0x8E:
                OpLD<0, 0x8E>(X);
                break;
            case 0x9E:
                OpLD<0, 0x9E>(X);
                break;
            case 0xAE:
                OpLD<0, 0xAE>(X);
                break;
            case 0xBE:
                OpLD<0, 0xBE>(X);
                break;
            case 0xCC:
                OpLD<0, 0xCC>(D);
                break;
            case 0xDC:
                OpLD<0, 0xDC>(D);
                break;
            case 0xEC:
                OpLD<0, 0xEC>(D);
                break;
            case 0xFC:
                OpLD<0, 0xFC>(D);
                break;
            case 0xCE:
                OpLD<0, 0xCE>(U);
                break;
            case 0xDE:
                OpLD<0, 0xDE>(U);
                break;
            case 0xEE:
                OpLD<0, 0xEE>(U);
                break;
            case 0xFE:
                OpLD<0, 0xFE>(U);
                break;

            // 8-bit ST
            case 0x97:
                OpST<0, 0x97>(A);
                break;
            case 0xA7:
                OpST<0, 0xA7>(A);
                break;
            case 0xB7:
                OpST<0, 0xB7>(A);
                break;
            case 0xD7:
                OpST<0, 0xD7>(B);
                break;
            case 0xE7:
                OpST<0, 0xE7>(B);
                break;
            case 0xF7:
                OpST<0, 0xF7>(B);
                break;
            // 16-bit ST
            case 0x9F:
                OpST<0, 0x9F>(X);
                break;
            case 0xAF:
                OpST<0, 0xAF>(X);
                break;
            case 0xBF:
                OpST<0, 0xBF>(X);
                break;
            case 0xDD:
                OpST<0, 0xDD>(D);
                break;
            case 0xDF:
                OpST<0, 0xDF>(U);
                break;
            case 0xED:
                OpST<0, 0xED>(D);
                break;
            case 0xEF:
                OpST<0, 0xEF>(U);
                break;
            case 0xFD:
                OpST<0, 0xFD>(D);
                break;
            case 0xFF:
                OpST<0, 0xFF>(U);
                break;

            case 0x8D: // BSR (branch to subroutine)
            {
                int8_t offset = ReadRelativeOffset8();
                Push16(S, PC);
                PC += offset;
            } break;
            case 0x17: // LBSR (long branch to subroutine)
            {
                int16_t offset = ReadRelativeOffset16();
                Push16(S, PC);
                PC += offset;
            } break;

            case 0x24: // BCC (branch if carry clear) or BHS (branch if higher or same)
                BranchIf([this] { return CC.Carry == 0; });
                break;
            case 0x25: // BCS (branch if carry set) or BLO (branch if lower)
                BranchIf([this] { return CC.Carry != 0; });
                break;
            case 0x27: // BEQ (branch if equal)
                BranchIf([this] { return CC.Zero != 0; });
                break;
            case 0x2C: // BGE (branch if greater or equal)
                BranchIf([this] { return (CC.Negative ^ CC.Overflow) == 0; });
                break;
            case 0x2E: // BGT (branch if greater)
                BranchIf([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) == 0; });
                break;
            case 0x22: // BHI (branch if higher)
                BranchIf([this] { return (CC.Carry | CC.Zero) == 0; });
                break;
            case 0x2F: // BLE (branch if less or equal)
                BranchIf([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) != 0; });
                break;
            case 0x23: // BLS (banch if lower or same)
                BranchIf([this] { return (CC.Carry | CC.Zero) != 0; });
                break;
            case 0x2D: // BLT (branch if less than)
                BranchIf([this] { return (CC.Negative ^ CC.Overflow) != 0; });
                break;
            case 0x2B: // BMI (brach if minus)
                BranchIf([this] { return CC.Negative != 0; });
                break;
            case 0x26: // BNE (branch if not equal)
                BranchIf([this] { return CC.Zero == 0; });
                break;
            case 0x2A: // BPL (branch if plus)
                BranchIf([this] { return CC.Negative == 0; });
                break;
            case 0x20: // BRA (branch always)
                BranchIf([this] { return true; });
                break;
            case 0x21: // BRN (branch never)
                BranchIf([this] { return false; });
                break;
            case 0x28: // BVC (branch if overflow clear)
                BranchIf([this] { return CC.Overflow == 0; });
                break;
            case 0x29: // BVS (branch if overflow set)
                BranchIf([this] { return CC.Overflow != 0; });
                break;

            case 0x1E: // EXG (exchange/swap register values)
            case 0x1F: // TFR (transfer register to register)
            {
                uint8_t postbyte = ReadPC8();
                assert(!!(postbyte & BITS(3)) ==
                       !!(postbyte & BITS(7))); // 8-bit to 8-bit or 16-bit to 16-bit only

                uint8_t src = (postbyte >> 4) & 0b111;
                uint8_t dst = postbyte & 0b111;

                if (postbyte & BITS(3)) {
                    assert(src < 4 && dst < 4); // Only first 4 are valid 8-bit register indices
                    uint8_t* const reg[]{&A, &B, &CC.Value, &DP};
                    if (cpuOp.opCode == 0x1E)
                        std::swap(*reg[dst], *reg[src]);
                    else
                        *reg[dst] = *reg[src];
                } else {
                    assert(src < 6 && dst < 6); // Only first 6 are valid 16-bit register indices
                    uint16_t* const reg[]{&D, &X, &Y, &U, &S, &PC};
                    if (cpuOp.opCode == 0x1E)
                        std::swap(*reg[dst], *reg[src]);
                    else
                        *reg[dst] = *reg[src];
                }
            } break;

            case 0x39: // RTS (return from subroutine)
            {
                PC = Pop16(S);
            } break;

            case 0x4F: // CLRA (clear A)
            {
                A = 0;
                CC.Negative = 0;
                CC.Zero = 1;
                CC.Overflow = 0;
                CC.Carry = 0;
            } break;
            case 0x5F: // CLRB (clear B)
            {
                B = 0;
                CC.Negative = 0;
                CC.Zero = 1;
                CC.Overflow = 0;
                CC.Carry = 0;
            } break;
            case 0x0F:
                OpCLR<0, 0x0F>();
                break;
            case 0x6F:
                OpCLR<0, 0x6F>();
                break;
            case 0x7F:
                OpCLR<0, 0x7F>();
                break;

            case 0x80:
                OpSUB<0, 0x80>(A);
                break;
            case 0x83:
                OpSUB<0, 0x83>(D);
                break;
            case 0x90:
                OpSUB<0, 0x90>(A);
                break;
            case 0x93:
                OpSUB<0, 0x93>(D);
                break;
            case 0xA0:
                OpSUB<0, 0xA0>(A);
                break;
            case 0xA3:
                OpSUB<0, 0xA3>(D);
                break;
            case 0xB0:
                OpSUB<0, 0xB0>(A);
                break;
            case 0xB3:
                OpSUB<0, 0xB3>(D);
                break;
            case 0xC0:
                OpSUB<0, 0xC0>(B);
                break;
            case 0xD0:
                OpSUB<0, 0xD0>(B);
                break;
            case 0xE0:
                OpSUB<0, 0xE0>(B);
                break;
            case 0xF0:
                OpSUB<0, 0xF0>(B);
                break;

            // INC
            case 0x0C:
                OpINC<0, 0x0C>();
                break;
            case 0x4C:
                OpINC<0, 0x4C>(A);
                break;
            case 0x5C:
                OpINC<0, 0x5C>(B);
            case 0x6C:
                OpINC<0, 0x6C>();
                break;
            case 0x7C:
                OpINC<0, 0x7C>();
                break;

            // DEC
            case 0x0A:
                OpDEC<0, 0x0A>();
                break;
            case 0x4A:
                OpDEC<0, 0x4A>(A);
                break;
            case 0x5A:
                OpDEC<0, 0x5A>(B);
            case 0x6A:
                OpDEC<0, 0x6A>();
                break;
            case 0x7A:
                OpDEC<0, 0x7A>();
                break;

            // JMP
            case 0x0E:
                OpJMP<0, 0x0E>();
                break;
            case 0x6E:
                OpJMP<0, 0x6E>();
                break;
            case 0x7E:
                OpJMP<0, 0x7E>();
                break;

            // PSH/PUL
            case 0x34: // PSHS
                OpPSH<0, 0x34>(S);
                break;
            case 0x35: // PULS
                OpPUL<0, 0x35>(S);
                break;
            case 0x36: // PSHU
                OpPSH<0, 0x36>(U);
                break;
            case 0x37: // PULU
                OpPUL<0, 0x37>(U);
                break;

            // TST
            case 0x0D:
                OpTST<0, 0x0D>();
                break;
            case 0x4D:
                OpTST<0, 0x4D>(A);
                break;
            case 0x5D:
                OpTST<0, 0x5D>(B);
                break;
            case 0x6D:
                OpTST<0, 0x6D>();
                break;
            case 0x7D:
                OpTST<0, 0x7D>();
                break;

            // ORA/ORB
            case 0x8A:
                OpOR<0, 0x8A>(A);
                break;
            case 0x9A:
                OpOR<0, 0x9A>(A);
                break;
            case 0xAA:
                OpOR<0, 0xAA>(A);
                break;
            case 0xBA:
                OpOR<0, 0xBA>(A);
                break;
            case 0xCA:
                OpOR<0, 0xCA>(B);
                break;
            case 0xDA:
                OpOR<0, 0xDA>(B);
                break;
            case 0xEA:
                OpOR<0, 0xEA>(B);
                break;
            case 0xFA:
                OpOR<0, 0xFA>(B);
                break;
            case 0x1A:
                OpOR<0, 0x1A>(CC.Value);
                break;

            // CMP
            case 0x81:
                OpCMP<0, 0x81>(A);
                break;
            case 0x8C:
                OpCMP<0, 0x8C>(X);
                break;
            case 0x91:
                OpCMP<0, 0x91>(A);
                break;
            case 0x9C:
                OpCMP<0, 0x9C>(X);
                break;
            case 0xA1:
                OpCMP<0, 0xA1>(A);
                break;
            case 0xAC:
                OpCMP<0, 0xAC>(X);
                break;
            case 0xB1:
                OpCMP<0, 0xB1>(A);
                break;
            case 0xBC:
                OpCMP<0, 0xBC>(X);
                break;
            case 0xC1:
                OpCMP<0, 0xC1>(B);
                break;
            case 0xD1:
                OpCMP<0, 0xD1>(B);
                break;
            case 0xE1:
                OpCMP<0, 0xE1>(B);
                break;
            case 0xF1:
                OpCMP<0, 0xF1>(B);
                break;

            // BIT
            case 0x85:
                OpBIT<0, 0x85>(A);
                break;
            case 0x95:
                OpBIT<0, 0x95>(A);
                break;
            case 0xA5:
                OpBIT<0, 0xA5>(A);
                break;
            case 0xB5:
                OpBIT<0, 0xB5>(A);
                break;
            case 0xC5:
                OpBIT<0, 0xC5>(B);
                break;
            case 0xD5:
                OpBIT<0, 0xD5>(B);
                break;
            case 0xE5:
                OpBIT<0, 0xE5>(B);
                break;
            case 0xF5:
                OpBIT<0, 0xF5>(B);
                break;

            default:
                UnhandledOp(cpuOp);
            }
            break;

        case 1:
            switch (cpuOp.opCode) {
            // 16-bit LD
            case 0x8E:
                OpLD<1, 0x8E>(Y);
                break;
            case 0x9E:
                OpLD<1, 0x9E>(Y);
                break;
            case 0xAE:
                OpLD<1, 0xAE>(Y);
                break;
            case 0xBE:
                OpLD<1, 0xBE>(Y);
                break;
            case 0xCE:
                OpLD<1, 0xCE>(S);
                break;
            case 0xDE:
                OpLD<1, 0xDE>(S);
                break;
            case 0xEE:
                OpLD<1, 0xEE>(S);
                break;
            case 0xFE:
                OpLD<1, 0xFE>(S);
                break;

            // 16-bit ST
            case 0x9F:
                OpST<1, 0x9F>(Y);
                break;
            case 0xAF:
                OpST<1, 0xAF>(Y);
                break;
            case 0xBF:
                OpST<1, 0xBF>(Y);
                break;
            case 0xDF:
                OpST<1, 0xDF>(S);
                break;
            case 0xEF:
                OpST<1, 0xEF>(S);
                break;
            case 0xFF:
                OpST<1, 0xFF>(S);
                break;

            // CMP
            case 0x83:
                OpCMP<1, 0x83>(D);
                break;
            case 0x8C:
                OpCMP<1, 0x8C>(Y);
                break;
            case 0x93:
                OpCMP<1, 0x93>(D);
                break;
            case 0x9C:
                OpCMP<1, 0x9C>(Y);
                break;
            case 0xA3:
                OpCMP<1, 0xA3>(D);
                break;
            case 0xAC:
                OpCMP<1, 0xAC>(Y);
                break;
            case 0xB3:
                OpCMP<1, 0xB3>(D);
                break;
            case 0xBC:
                OpCMP<1, 0xBC>(Y);
                break;

            default:
                UnhandledOp(cpuOp);
            }
            break;

        case 2:
            switch (cpuOp.opCode) {
            case 0x00:
                UnhandledOp(cpuOp);
                break;

            // CMP
            case 0x83:
                OpCMP<2, 0x83>(U);
                break;
            case 0x8C:
                OpCMP<2, 0x8C>(S);
                break;
            case 0x93:
                OpCMP<2, 0x93>(U);
                break;
            case 0x9C:
                OpCMP<2, 0x9C>(S);
                break;
            case 0xA3:
                OpCMP<2, 0xA3>(U);
                break;
            case 0xAC:
                OpCMP<2, 0xAC>(S);
                break;
            case 0xB3:
                OpCMP<2, 0xB3>(U);
                break;
            case 0xBC:
                OpCMP<2, 0xBC>(S);
                break;

            default:
                UnhandledOp(cpuOp);
            }
            break;
        }

        return m_cycles;
    }
};

Cpu::Cpu()
    : m_impl(std::make_unique<CpuImpl>()) {}

Cpu::~Cpu() {}

void Cpu::Init(MemoryBus& memoryBus) {
    m_impl->Init(memoryBus);
}

void Cpu::Reset(uint16_t initialPC) {
    m_impl->Reset(initialPC);
}

cycles_t Cpu::ExecuteInstruction() {
    return m_impl->ExecuteInstruction();
}

const CpuRegisters& Cpu::Registers() {
    return *m_impl;
}
