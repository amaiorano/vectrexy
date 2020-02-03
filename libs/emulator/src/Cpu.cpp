#include "emulator/Cpu.h"
#include "core/BitOps.h"
#include "core/ErrorHandler.h"
#include "emulator/CpuHelpers.h"
#include "emulator/CpuOpCodes.h"
#include "emulator/MemoryBus.h"
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

    template <typename T>
    constexpr uint8_t CalcZero(T v) {
        return v == 0;
    }

    constexpr uint8_t CalcNegative(uint8_t v) { return (v & BITS(7)) != 0; }
    constexpr uint8_t CalcNegative(uint16_t v) { return (v & BITS(15)) != 0; }

    constexpr uint8_t CalcCarry(uint16_t r) { return (r & 0xFF00) != 0; }
    constexpr uint8_t CalcCarry(uint32_t r) { return (r & 0xFFFF'0000) != 0; }
    uint8_t CalcCarry(uint8_t r) = delete; // Result must be larger than 8 or 16 bits

    constexpr uint8_t CalcHalfCarryFromAdd(uint8_t a, uint8_t b, uint8_t carry) {
        return (((a & 0x0F) + (b & 0x0F) + carry) & 0x10) != 0;
    }
    uint8_t CalcHalfCarryFromAdd(uint16_t a, uint16_t b,
                                 uint8_t carry) = delete; // Only 8-bit add computes half-carry

    constexpr uint8_t CalcOverflow(uint8_t a, uint8_t b, uint16_t r) {
        // Given r = a + b, overflow occurs if both a and b are negative and r is positive, or both
        // a and b are positive and r is negative. Looking at sign bits of a, b, and r, overflow
        // occurs when 0 0 1 or 1 1 0.
        return (((uint16_t)a ^ r) & ((uint16_t)b ^ r) & BITS(7)) != 0;
    }
    constexpr uint8_t CalcOverflow(uint16_t a, uint16_t b, uint32_t r) {
        return (((uint32_t)a ^ r) & ((uint32_t)b ^ r) & BITS(15)) != 0;
    }
    template <typename T>
    uint8_t CalcOverflow(T a, T b, uint8_t r) = delete; // Result must be larger than 8 or 16 bits

    namespace InterruptVector {
        enum Type : uint16_t {
            Swi2 = 0xFFF2,
            Swi3 = 0xFFF4,
            Firq = 0xFFF6,
            Irq = 0xFFF8,
            Swi = 0xFFFA,
            Nmi = 0xFFFC,
            Reset = 0xFFFE,
        };
    } // namespace InterruptVector

} // namespace

class CpuImpl : public CpuRegisters {
public:
    MemoryBus* m_memoryBus{};
    cycles_t m_cycles{};
    bool m_waitingForInterrupts{}; // Set by CWAI

    void Init(MemoryBus& memoryBus) { m_memoryBus = &memoryBus; }

    void AddCycles(cycles_t cycles) {
        m_cycles += cycles;
        m_memoryBus->AddSyncCycles(cycles);
    }

    void Reset() {
        X = 0;
        Y = 0;
        U = 0;
        S = 0; // BIOS will init this to 0xCBEA, which is the last byte of programmer-usable RAM
        DP = 0;

        CC.Value = 0;
        CC.InterruptMask = 1;
        CC.FastInterruptMask = 1;

        // Read initial location from last 2 bytes of address-space (is 0xF000 on Vectrex)
        PC = Read16(InterruptVector::Reset);

        m_waitingForInterrupts = false;
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

        if ((postbyte & BITS(7)) == 0) // (+/- 4 bit offset),R
        {
            // postbyte is a 5 bit two's complement number we convert to 8 bit.
            // So if bit 4 is set (sign bit), we extend the sign bit by turning on bits 6,7,8;
            int8_t offset = postbyte & 0b0001'1111;
            if (postbyte & BITS(4))
                offset |= 0b1110'0000;
            EA = RegisterSelect(postbyte) + offset;
            supportsIndirect = false;
            AddCycles(1);
        } else {
            switch (postbyte & 0b1111) {
            case 0b0000: { // ,R+
                auto& reg = RegisterSelect(postbyte);
                EA = reg;
                reg += 1;
                supportsIndirect = false;
                AddCycles(2);
            } break;
            case 0b0001: { // ,R++
                auto& reg = RegisterSelect(postbyte);
                EA = reg;
                reg += 2;
                AddCycles(3);
            } break;
            case 0b0010: { // ,-R
                auto& reg = RegisterSelect(postbyte);
                reg -= 1;
                EA = reg;
                supportsIndirect = false;
                AddCycles(2);
            } break;
            case 0b0011: { // ,--R
                auto& reg = RegisterSelect(postbyte);
                reg -= 2;
                EA = reg;
                AddCycles(3);
            } break;
            case 0b0100: // ,R
                EA = RegisterSelect(postbyte);
                break;
            case 0b0101: // (+/- B),R
                EA = RegisterSelect(postbyte) + S16(B);
                AddCycles(1);
                break;
            case 0b0110: // (+/- A),R
                EA = RegisterSelect(postbyte) + S16(A);
                AddCycles(1);
                break;
            case 0b0111:
                ErrorHandler::Undefined("Illegal indexed instruction post-byte\n");
                break;
            case 0b1000: { // (+/- 7 bit offset),R
                uint8_t postbyte2 = ReadPC8();
                EA = RegisterSelect(postbyte) + S16(postbyte2);
                AddCycles(1);
            } break;
            case 0b1001: { // (+/- 15 bit offset),R
                uint8_t postbyte2 = ReadPC8();
                uint8_t postbyte3 = ReadPC8();
                EA = RegisterSelect(postbyte) + CombineToS16(postbyte2, postbyte3);
                AddCycles(4);
            } break;
            case 0b1010:
                ErrorHandler::Undefined("Illegal indexed instruction post-byte\n");
                break;
            case 0b1011: // (+/- D),R
                EA = RegisterSelect(postbyte) + S16(D);
                AddCycles(4);
                break;
            case 0b1100: { // (+/- 7 bit offset),PC
                uint8_t postbyte2 = ReadPC8();
                EA = PC + S16(postbyte2);
                AddCycles(1);
            } break;
            case 0b1101: { // (+/- 15 bit offset),PC
                uint8_t postbyte2 = ReadPC8();
                uint8_t postbyte3 = ReadPC8();
                EA = PC + CombineToS16(postbyte2, postbyte3);
                AddCycles(5);
            } break;
            case 0b1110:
                ErrorHandler::Undefined("Illegal indexed instruction post-byte\n");
                break;
            case 0b1111: { // [address] (Indirect-only)
                uint8_t postbyte2 = ReadPC8();
                uint8_t postbyte3 = ReadPC8();
                EA = CombineToS16(postbyte2, postbyte3);
                AddCycles(2);
            } break;
            default:
                ErrorHandler::Undefined("Illegal indexed instruction post-byte\n");
                break;
            }
        }

        if (supportsIndirect && (postbyte & BITS(4))) {
            uint8_t msb = m_memoryBus->Read(EA);
            uint8_t lsb = m_memoryBus->Read(EA + 1);
            EA = CombineToU16(msb, lsb);
            AddCycles(3);
        }

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
        ErrorHandler::Undefined("Not implemented for addressing mode\n");
        return 0xFFFF;
    }

    // Read CPU op's value (8/16 bit) either directly or indirectly (via EA) depending on addressing
    // mode Default template assumes operand is EA and de-refs it
    template <AddressingMode addressingMode>
    uint16_t ReadOperandValue16() {
        auto EA = ReadEA16<addressingMode>();
        return Read16(EA);
    }

    template <AddressingMode addressingMode>
    uint8_t ReadOperandValue8() {
        auto EA = ReadEA16<addressingMode>();
        return Read8(EA);
    }

    // Read CPU op's relative offset from next 8/16 bits
    int8_t ReadRelativeOffset8() { return static_cast<int8_t>(ReadPC8()); }
    int16_t ReadRelativeOffset16() { return static_cast<int16_t>(ReadPC16()); }

    template <int page, uint8_t opCode>
    void OpLD(uint8_t& targetReg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        CC.Negative = CalcNegative(value);
        CC.Zero = CalcZero(value);
        CC.Overflow = 0;
        targetReg = value;
    }

    template <int page, uint8_t opCode>
    void OpLD(uint16_t& targetReg) {
        uint16_t value = ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>();
        CC.Negative = CalcNegative(value);
        CC.Zero = CalcZero(value);
        CC.Overflow = 0;
        targetReg = value;
    }

    template <int page, uint8_t opCode>
    void OpST(const uint8_t& sourceReg) {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        m_memoryBus->Write(EA, sourceReg);
        CC.Negative = CalcNegative(sourceReg);
        CC.Zero = CalcZero(sourceReg);
        CC.Overflow = 0;
    }

    template <int page, uint8_t opCode>
    void OpST(const uint16_t& sourceReg) {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        m_memoryBus->Write(EA, U8(sourceReg >> 8));       // High
        m_memoryBus->Write(EA + 1, U8(sourceReg & 0xFF)); // Low
        CC.Negative = CalcNegative(sourceReg);
        CC.Zero = CalcZero(sourceReg);
        CC.Overflow = 0;
    }

    template <int page, uint8_t opCode>
    void OpLEA(uint16_t& reg) {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        reg = EA;
        // Zero flag not affected by LEAU/LEAS
        if (&reg == &X || &reg == &Y) {
            CC.Zero = (reg == 0);
        }
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

    void OpCLR(uint8_t& reg) {
        reg = 0;
        CC.Negative = 0;
        CC.Zero = 1;
        CC.Overflow = 0;
        CC.Carry = 0;
    }

    static uint8_t AddImpl(uint8_t a, uint8_t b, uint8_t carry, ConditionCode& CC) {
        uint16_t r16 = U16(a) + U16(b) + U16(carry);
        CC.HalfCarry = CalcHalfCarryFromAdd(a, b, carry);
        CC.Carry = CalcCarry(r16);
        CC.Overflow = CalcOverflow(a, b, r16);
        uint8_t r8 = U8(r16);
        CC.Zero = CalcZero(r8);
        CC.Negative = CalcNegative(r8);
        return r8;
    }
    static uint16_t AddImpl(uint16_t a, uint16_t b, uint16_t carry, ConditionCode& CC) {
        uint32_t r32 = U16(a) + U16(b) + U16(carry);
        // CC.HalfCarry = CalcHalfCarryFromAdd(a, b, carry);
        CC.Carry = CalcCarry(r32);
        CC.Overflow = CalcOverflow(a, b, r32);
        uint16_t r16 = U16(r32);
        CC.Zero = CalcZero(r16);
        CC.Negative = CalcNegative(r16);
        return r16;
    }

    static uint8_t SubtractImpl(uint8_t a, uint8_t b, uint8_t carry, ConditionCode& CC) {
        auto result = AddImpl(a, ~b, 1 - carry, CC);
        CC.Carry = !CC.Carry; // Carry is set if no borrow occurs
        return result;
    }
    static uint16_t SubtractImpl(uint16_t a, uint16_t b, uint16_t carry, ConditionCode& CC) {
        auto result = AddImpl(a, ~b, 1 - carry, CC);
        CC.Carry = !CC.Carry; // Carry is set if no borrow occurs
        return result;
    }

    // ADDA, ADDB
    template <int page, uint8_t opCode>
    void OpADD(uint8_t& reg) {
        uint8_t b = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        reg = AddImpl(reg, b, 0, CC);
    }

    // ADDD
    template <int page, uint8_t opCode>
    void OpADD(uint16_t& reg) {
        uint16_t b = ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>();
        reg = AddImpl(reg, b, 0, CC);
    }

    // ADCA, ADCB
    template <int page, uint8_t opCode>
    void OpADC(uint8_t& reg) {
        uint8_t b = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        reg = AddImpl(reg, b, CC.Carry, CC);
    }

    // SUBA, SUBB
    template <int page, uint8_t opCode>
    void OpSUB(uint8_t& reg) {
        reg = SubtractImpl(reg, ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>(), 0, CC);
    }

    // SUBD
    template <int page, uint8_t opCode>
    void OpSUB(uint16_t& reg) {
        reg = SubtractImpl(reg, ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>(), 0, CC);
    }

    // SBCA, SBCB
    template <int page, uint8_t opCode>
    void OpSBC(uint8_t& reg) {
        reg = SubtractImpl(reg, ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>(), CC.Carry,
                           CC);
    }

    // MUL
    template <int page, uint8_t opCode>
    void OpMUL() {
        uint16_t result = A * B;
        CC.Zero = CalcZero(result);
        CC.Carry = TestBits01(result, BITS(7)); // Because bitwise multiply
        D = result;
    }

    template <int page, uint8_t opCode>
    void OpSEX() {
        A = TestBits(B, BITS(7)) ? 0xFF : 0;
        CC.Negative = CalcNegative(D);
        CC.Zero = CalcZero(D);
    }

    // NEGA, NEGB
    template <int page, uint8_t opCode>
    void OpNEG(uint8_t& value) {
        // Negating is 0 - value
        value = SubtractImpl(0, value, 0, CC);
    }

    // NEG <address>
    template <int page, uint8_t opCode>
    void OpNEG() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpNEG<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    // INCA, INCB
    template <int page, uint8_t opCode>
    void OpINC(uint8_t& value) {
        uint8_t origValue = value;
        ++value;
        CC.Overflow = origValue == 0b0111'1111;
        CC.Zero = CalcZero(value);
        CC.Negative = CalcNegative(value);
    }

    // INC <address>
    template <int page, uint8_t opCode>
    void OpINC() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpINC<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    // DECA, DECB
    template <int page, uint8_t opCode>
    void OpDEC(uint8_t& value) {
        uint8_t origValue = value;
        --value;
        CC.Overflow = origValue == 0b1000'0000; // Could also set to (value == 0b01111'1111)
        CC.Zero = CalcZero(value);
        CC.Negative = CalcNegative(value);
    }

    // DEC <address>
    template <int page, uint8_t opCode>
    void OpDEC() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpDEC<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpASR(uint8_t& value) {
        auto origValue = value;
        value = (origValue & 0b1000'0000) | (value >> 1);
        CC.Zero = CalcZero(value);
        CC.Negative = CalcNegative(value);
        CC.Carry = origValue & 0b0000'0001;
    }

    template <int page, uint8_t opCode>
    void OpASR() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpASR<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpLSR(uint8_t& value) {
        auto origValue = value;
        value = (value >> 1);
        CC.Zero = CalcZero(value);
        CC.Negative = 0; // Bit 7 always shifted out
        CC.Carry = origValue & 0b0000'0001;
    }

    template <int page, uint8_t opCode>
    void OpLSR() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpLSR<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpROL(uint8_t& value) {
        uint8_t result = (value << 1) | CC.Carry;
        CC.Carry = TestBits01(value, BITS(7));
        //@TODO: Can we use CalcOverflow(value) instead?
        CC.Overflow = ((value & BITS(7)) ^ ((value & BITS(6)) << 1)) != 0;
        CC.Negative = CalcNegative(result);
        CC.Zero = CalcZero(result);
        value = result;
    }

    template <int page, uint8_t opCode>
    void OpROL() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpROL<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpROR(uint8_t& value) {
        uint8_t result = (CC.Carry << 7) | (value >> 1);
        CC.Carry = TestBits01(value, BITS(0));
        CC.Negative = CalcNegative(result);
        CC.Zero = CalcZero(result);
        value = result;
    }

    template <int page, uint8_t opCode>
    void OpROR() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpROR<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpCOM(uint8_t& value) {
        value = ~value;
        CC.Negative = CalcNegative(value);
        CC.Zero = CalcZero(value);
        CC.Overflow = 0;
        CC.Carry = 1;
    }

    template <int page, uint8_t opCode>
    void OpCOM() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpCOM<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpASL(uint8_t& value) {
        // Shifting left is same as adding value + value (aka value * 2)
        value = AddImpl(value, value, 0, CC);
    }

    template <int page, uint8_t opCode>
    void OpASL() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        uint8_t value = m_memoryBus->Read(EA);
        OpASL<page, opCode>(value);
        m_memoryBus->Write(EA, value);
    }

    template <int page, uint8_t opCode>
    void OpJMP() {
        uint16_t EA = ReadEA16<LookupCpuOp(page, opCode).addrMode>();
        PC = EA;
    }

    template <int page, uint8_t opCode>
    void OpPSH(uint16_t& stackReg) {
        ASSERT(&stackReg == &S || &stackReg == &U);
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

        // 1 cycle per byte pushed
        AddCycles(NumBitsSet(ReadBits(value, BITS(0, 1, 2, 3))));
        AddCycles(NumBitsSet(ReadBits(value, BITS(4, 5, 6, 7))) * 2);
    }

    template <int page, uint8_t opCode>
    void OpPUL(uint16_t& stackReg) {
        ASSERT(&stackReg == &S || &stackReg == &U);
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

        // 1 cycle per byte pulled
        AddCycles(NumBitsSet(ReadBits(value, BITS(0, 1, 2, 3))));
        AddCycles(NumBitsSet(ReadBits(value, BITS(4, 5, 6, 7))) * 2);
    }

    template <int page, uint8_t opCode>
    void OpTST(const uint8_t& value) {
        CC.Negative = CalcNegative(value);
        CC.Zero = CalcZero(value);
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
            CC.Negative = CalcNegative(reg);
            CC.Zero = CalcZero(reg);
            CC.Overflow = 0;
        }
    }

    template <int page, uint8_t opCode>
    void OpAND(uint8_t& reg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        reg = reg & value;
        // For ANDCC, we don't update CC. @TODO: separate function?
        if (&reg != &CC.Value) {
            CC.Negative = CalcNegative(reg);
            CC.Zero = CalcZero(reg);
            CC.Overflow = 0;
        }
    }

    template <int page, uint8_t opCode>
    void OpEOR(uint8_t& reg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        reg ^= value;
        CC.Negative = CalcNegative(reg);
        CC.Zero = CalcZero(reg);
        CC.Overflow = 0;
    }

    template <int page, uint8_t opCode>
    void OpRTI() {
        bool poppedEntire{};
        PopCCState(poppedEntire);
        AddCycles(poppedEntire ? 15 : 6);
    }

    template <int page, uint8_t opCode>
    void OpCWAI() {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        CC.Value = CC.Value & value;
        PushCCState(true);
        ASSERT(!m_waitingForInterrupts);
        m_waitingForInterrupts = true;
    }

    template <int page, uint8_t opCode>
    void OpCMP(const uint8_t& reg) {
        // Subtract to update CC, but discard result
        uint8_t discard =
            SubtractImpl(reg, ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>(), 0, CC);
        (void)discard;
    }

    template <int page, uint8_t opCode>
    void OpCMP(const uint16_t& reg) {
        uint16_t discard =
            SubtractImpl(reg, ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>(), 0, CC);
        (void)discard;
    }

    template <int page, uint8_t opCode>
    void OpBIT(const uint8_t& reg) {
        uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
        uint8_t result = reg & value;
        CC.Negative = CalcNegative(result);
        CC.Zero = CalcZero(result);
        CC.Overflow = 0;
    }

    template <typename CondFunc>
    void OpBranch(CondFunc condFunc) {
        int8_t offset = ReadRelativeOffset8();
        if (condFunc()) {
            PC += offset;
        }
    }

    template <typename CondFunc>
    void OpLongBranch(CondFunc condFunc) {
        int16_t offset = ReadRelativeOffset16();
        if (condFunc()) {
            PC += offset;
            AddCycles(1); // Extra cycle if branch is taken
        }
    }

    void OpLBRA() {
        int16_t offset = ReadRelativeOffset16();
        PC += offset;
    }

    void OpBSR() {
        int8_t offset = ReadRelativeOffset8();
        Push16(S, PC);
        PC += offset;
    }

    void OpLBSR() {
        int16_t offset = ReadRelativeOffset16();
        Push16(S, PC);
        PC += offset;
    }

    void OpRTS() { PC = Pop16(S); }

    void ExchangeOrTransfer(bool exchange) {
        uint8_t postbyte = ReadPC8();
        ASSERT(!!(postbyte & BITS(3)) ==
               !!(postbyte & BITS(7))); // 8-bit to 8-bit or 16-bit to 16-bit only

        uint8_t src = (postbyte >> 4) & 0b111;
        uint8_t dst = postbyte & 0b111;

        if (postbyte & BITS(3)) {
            ASSERT(src < 4 && dst < 4); // Only first 4 are valid 8-bit register indices
            uint8_t* const reg[]{&A, &B, &CC.Value, &DP};
            if (exchange)
                std::swap(*reg[dst], *reg[src]);
            else
                *reg[dst] = *reg[src];
        } else {
            ASSERT(src < 6 && dst < 6); // Only first 6 are valid 16-bit register indices
            uint16_t* const reg[]{&D, &X, &Y, &U, &S, &PC};
            if (exchange)
                std::swap(*reg[dst], *reg[src]);
            else
                *reg[dst] = *reg[src];
        }
    }

    void OpEXG() { ExchangeOrTransfer(true); }

    void OpTFR() { ExchangeOrTransfer(false); }

    void OpABX() { X += B; }

    void OpDAA() {
        // Extract least and most siginifant nibbles
        uint8_t lsn = A & 0b0000'1111;
        uint8_t msn = (A & 0b1111'0000) >> 4;

        // Compute correction factors
        uint8_t cfLsn = ((CC.HalfCarry == 1) || (lsn > 9)) ? 6 : 0;
        uint8_t cfMsn = ((CC.Carry == 1) || (msn > 9) || (msn > 8 && lsn > 9)) ? 6 : 0;
        uint8_t adjust = (cfMsn << 4) | cfLsn;
        uint16_t r16 = U16(A) + U16(adjust);
        A = U8(r16);
        CC.Negative = CalcNegative(A);
        CC.Zero = CalcZero(A);
        CC.Carry = (CC.Carry == 1) || CalcCarry(r16);
    }

    void OpRESET() {
        // I think this right?
        Reset();
    }

    void OpSWI(InterruptVector::Type type) {
        assert(type == InterruptVector::Swi || type == InterruptVector::Swi2 ||
               type == InterruptVector::Swi3);
        PushCCState(true);
        if (type == InterruptVector::Swi) {
            CC.InterruptMask = 1;
            CC.FastInterruptMask = 1;
        }
        PC = Read16(type);
    }

    void PushCCState(bool entire) {
        CC.Entire = entire ? 1 : 0;

        Push16(S, PC);
        Push16(S, U);
        Push16(S, Y);
        Push16(S, X);
        Push8(S, DP);
        Push8(S, B);
        Push8(S, A);
        Push8(S, CC.Value);
    }

    void PopCCState(bool& poppedEntire) {
        CC.Value = Pop8(S);
        poppedEntire = CC.Entire != 0;
        if (CC.Entire) {
            A = Pop8(S);
            B = Pop8(S);
            DP = Pop8(S);
            X = Pop16(S);
            Y = Pop16(S);
            U = Pop16(S);
            PC = Pop16(S);
        } else {
            PC = Pop16(S);
        }
        //@TODO: CC.Entire = 0; ?
    }

    cycles_t ExecuteInstruction(bool irqEnabled, bool firqEnabled) {
        m_cycles = 0;
        DoExecuteInstruction(irqEnabled, firqEnabled);
        return m_cycles;
    }

    void DoExecuteInstruction(bool irqEnabled, bool firqEnabled) {
        auto UnhandledOp = [](const CpuOp& cpuOp) {
            ErrorHandler::Undefined("Unhandled Op: %s\n", cpuOp.name);
        };

        // Just for debugging, keep a copy in case we assert
        const auto currInstructionPC = PC;
        (void)currInstructionPC;

        if (m_waitingForInterrupts) {
            if (irqEnabled && (CC.InterruptMask == 0)) {
                m_waitingForInterrupts = false;
                CC.InterruptMask = 1;
                PC = Read16(InterruptVector::Irq);
                return;

            } else if (firqEnabled && (CC.FastInterruptMask == 0)) {
                ErrorHandler::Unsupported("Implement FIRQ after CWAI\n");
                AddCycles(10);
                return;

            } else {
                // Technically, no cycles are consumed while waiting for interrupts; but we add a
                // nominal amount of cycles to make sure the emulator updates other components with
                // non-zero cycles while in this state.
                AddCycles(10);
                return;
            }
        }

        if (irqEnabled && (CC.InterruptMask == 0)) {
            PushCCState(true);
            CC.InterruptMask = 1;
            PC = Read16(InterruptVector::Irq);
            AddCycles(19);
            return;
        }

        if (firqEnabled && (CC.FastInterruptMask == 0)) {
            ErrorHandler::Unsupported("Implement FIRQ\n");
            return;
        }

        // Read op code byte and page
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

        ASSERT_MSG(cpuOp.cycles >= 0, "TODO: look at how to handle cycles for instruction: %s",
                   cpuOp.name);
        AddCycles(cpuOp.cycles); // Base cycles for this instruction

        if (cpuOp.addrMode == AddressingMode::Illegal) {
            ErrorHandler::Undefined("Illegal instruction at $%04x, opcode: %02x, page: %d\n",
                                    currInstructionPC, opCodeByte, cpuOpPage);
            return;
        }

        ASSERT(cpuOp.addrMode != AddressingMode::Variant &&
               "Page 1/2 instruction, should have read next byte by now");

        switch (cpuOpPage) {
        case 0:
            switch (cpuOp.opCode) {
            case 0x3E:
                OpRESET();
                break;
            case 0x3F:
                OpSWI(InterruptVector::Swi);
                break;

            case 0x12:
                // NOP
                break;

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

            case 0x30:
                OpLEA<0, 0x30>(X);
                break;
            case 0x31:
                OpLEA<0, 0x31>(Y);
                break;
            case 0x32:
                OpLEA<0, 0x32>(S);
                break;
            case 0x33:
                OpLEA<0, 0x33>(U);
                break;

            case 0x8D:
                OpBSR();
                break;
            case 0x17:
                OpLBSR();
                break;

            case 0x19:
                OpDAA();
                break;

            case 0x20: // BRA (branch always)
                OpBranch([] { return true; });
                break;
            case 0x21: // BRN (branch never)
                OpBranch([] { return false; });
                break;
            case 0x22: // BHI (branch if higher)
                OpBranch([this] { return (CC.Carry | CC.Zero) == 0; });
                break;
            case 0x23: // BLS (banch if lower or same)
                OpBranch([this] { return (CC.Carry | CC.Zero) != 0; });
                break;
            case 0x24: // BCC (branch if carry clear) or BHS (branch if higher or same)
                OpBranch([this] { return CC.Carry == 0; });
                break;
            case 0x25: // BCS (branch if carry set) or BLO (branch if lower)
                OpBranch([this] { return CC.Carry != 0; });
                break;
            case 0x26: // BNE (branch if not equal)
                OpBranch([this] { return CC.Zero == 0; });
                break;
            case 0x27: // BEQ (branch if equal)
                OpBranch([this] { return CC.Zero != 0; });
                break;
            case 0x28: // BVC (branch if overflow clear)
                OpBranch([this] { return CC.Overflow == 0; });
                break;
            case 0x29: // BVS (branch if overflow set)
                OpBranch([this] { return CC.Overflow != 0; });
                break;
            case 0x2A: // BPL (branch if plus)
                OpBranch([this] { return CC.Negative == 0; });
                break;
            case 0x2B: // BMI (brach if minus)
                OpBranch([this] { return CC.Negative != 0; });
                break;
            case 0x2C: // BGE (branch if greater or equal)
                OpBranch([this] { return (CC.Negative ^ CC.Overflow) == 0; });
                break;
            case 0x2D: // BLT (branch if less than)
                OpBranch([this] { return (CC.Negative ^ CC.Overflow) != 0; });
                break;
            case 0x2E: // BGT (branch if greater)
                OpBranch([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) == 0; });
                break;
            case 0x2F: // BLE (branch if less or equal)
                OpBranch([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) != 0; });
                break;

            case 0x16:
                // Note: LBRA is in table 0, while all other long branch instructions are in table 1
                OpLBRA();
                break;

            case 0x1E: // EXG (exchange/swap register values)
                OpEXG();
                break;

            case 0x1F:
                OpTFR();
                break;

            case 0x3A:
                OpABX();
                break;

            case 0x39:
                OpRTS();
                break;

            case 0x4F:
                OpCLR(A);
                break;
            case 0x5F:
                OpCLR(B);
                break;
            case 0x0F:
                OpCLR<0, 0x0F>();
                break;
            case 0x6F:
                OpCLR<0, 0x6F>();
                break;
            case 0x7F:
                OpCLR<0, 0x7F>();
                break;

            case 0x8B:
                OpADD<0, 0x8B>(A);
                break;
            case 0x9B:
                OpADD<0, 0x9B>(A);
                break;
            case 0xAB:
                OpADD<0, 0xAB>(A);
                break;
            case 0xBB:
                OpADD<0, 0xBB>(A);
                break;
            case 0xC3:
                OpADD<0, 0xC3>(D);
                break;
            case 0xCB:
                OpADD<0, 0xCB>(B);
                break;
            case 0xD3:
                OpADD<0, 0xD3>(D);
                break;
            case 0xDB:
                OpADD<0, 0xDB>(B);
                break;
            case 0xE3:
                OpADD<0, 0xE3>(D);
                break;
            case 0xEB:
                OpADD<0, 0xEB>(B);
                break;
            case 0xF3:
                OpADD<0, 0xF3>(D);
                break;
            case 0xFB:
                OpADD<0, 0xFB>(B);
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

            case 0x89:
                OpADC<0, 0x89>(A);
                break;
            case 0x99:
                OpADC<0, 0x99>(A);
                break;
            case 0xA9:
                OpADC<0, 0xA9>(A);
                break;
            case 0xB9:
                OpADC<0, 0xB9>(A);
                break;
            case 0xC9:
                OpADC<0, 0xC9>(B);
                break;
            case 0xD9:
                OpADC<0, 0xD9>(B);
                break;
            case 0xE9:
                OpADC<0, 0xE9>(B);
                break;
            case 0xF9:
                OpADC<0, 0xF9>(B);
                break;

            case 0x82:
                OpSBC<0, 0x82>(A);
                break;
            case 0x92:
                OpSBC<0, 0x92>(A);
                break;
            case 0xA2:
                OpSBC<0, 0xA2>(A);
                break;
            case 0xB2:
                OpSBC<0, 0xB2>(A);
                break;
            case 0xC2:
                OpSBC<0, 0xC2>(B);
                break;
            case 0xD2:
                OpSBC<0, 0xD2>(B);
                break;
            case 0xE2:
                OpSBC<0, 0xE2>(B);
                break;
            case 0xF2:
                OpSBC<0, 0xF2>(B);
                break;

            case 0x3D:
                OpMUL<0, 0x3D>();
                break;

            case 0x1D:
                OpSEX<0, 0x1D>();
                break;

            // NEG
            case 0x00:
                OpNEG<0, 0x00>();
                break;
            case 0x40:
                OpNEG<0, 0x40>(A);
                break;
            case 0x50:
                OpNEG<0, 0x50>(B);
                break;
            case 0x60:
                OpNEG<0, 0x60>();
                break;
            case 0x70:
                OpNEG<0, 0x70>();
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
                break;
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
                break;
            case 0x6A:
                OpDEC<0, 0x6A>();
                break;
            case 0x7A:
                OpDEC<0, 0x7A>();
                break;

            // ASR
            case 0x07:
                OpASR<0, 0x07>();
                break;
            case 0x47:
                OpASR<0, 0x47>(A);
                break;
            case 0x57:
                OpASR<0, 0x57>(B);
                break;
            case 0x67:
                OpASR<0, 0x67>();
                break;
            case 0x77:
                OpASR<0, 0x77>();
                break;

            // LSL/ASL
            case 0x08:
                OpASL<0, 0x08>();
                break;
            case 0x48:
                OpASL<0, 0x48>(A);
                break;
            case 0x58:
                OpASL<0, 0x58>(B);
                break;
            case 0x68:
                OpASL<0, 0x68>();
                break;
            case 0x78:
                OpASL<0, 0x78>();
                break;

            // LSR
            case 0x04:
                OpLSR<0, 0x04>();
                break;
            case 0x44:
                OpLSR<0, 0x44>(A);
                break;
            case 0x54:
                OpLSR<0, 0x54>(B);
                break;
            case 0x64:
                OpLSR<0, 0x64>();
                break;
            case 0x74:
                OpLSR<0, 0x74>();
                break;

            // ROL
            case 0x09:
                OpROL<0, 0x09>();
                break;
            case 0x49:
                OpROL<0, 0x49>(A);
                break;
            case 0x59:
                OpROL<0, 0x59>(B);
                break;
            case 0x69:
                OpROL<0, 0x69>();
                break;
            case 0x79:
                OpROL<0, 0x79>();
                break;

            // ROR
            case 0x06:
                OpROR<0, 0x06>();
                break;
            case 0x46:
                OpROR<0, 0x46>(A);
                break;
            case 0x56:
                OpROR<0, 0x56>(B);
                break;
            case 0x66:
                OpROR<0, 0x66>();
                break;
            case 0x76:
                OpROR<0, 0x76>();
                break;

            // COM
            case 0x03:
                OpCOM<0, 0x03>();
                break;
            case 0x43:
                OpCOM<0, 0x43>(A);
                break;
            case 0x53:
                OpCOM<0, 0x53>(B);
                break;
            case 0x63:
                OpCOM<0, 0x63>();
                break;
            case 0x73:
                OpCOM<0, 0x73>();
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

            // AND
            case 0x1C:
                OpAND<0, 0x1C>(CC.Value);
                break;
            case 0x84:
                OpAND<0, 0x84>(A);
                break;
            case 0x94:
                OpAND<0, 0x94>(A);
                break;
            case 0xA4:
                OpAND<0, 0xA4>(A);
                break;
            case 0xB4:
                OpAND<0, 0xB4>(A);
                break;
            case 0xC4:
                OpAND<0, 0xC4>(B);
                break;
            case 0xD4:
                OpAND<0, 0xD4>(B);
                break;
            case 0xE4:
                OpAND<0, 0xE4>(B);
                break;
            case 0xF4:
                OpAND<0, 0xF4>(B);
                break;

            // EOR
            case 0x88:
                OpEOR<0, 0x88>(A);
                break;
            case 0x98:
                OpEOR<0, 0x98>(A);
                break;
            case 0xA8:
                OpEOR<0, 0xA8>(A);
                break;
            case 0xB8:
                OpEOR<0, 0xB8>(A);
                break;
            case 0xC8:
                OpEOR<0, 0xC8>(B);
                break;
            case 0xD8:
                OpEOR<0, 0xD8>(B);
                break;
            case 0xE8:
                OpEOR<0, 0xE8>(B);
                break;
            case 0xF8:
                OpEOR<0, 0xF8>(B);
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

            case 0x3B:
                OpRTI<0, 0x3B>();
                break;
            case 0x3C:
                OpCWAI<0, 0x3C>();
                break;

            default:
                UnhandledOp(cpuOp);
            }
            break;

        case 1:
            switch (cpuOp.opCode) {
            case 0x3F:
                OpSWI(InterruptVector::Swi2);
                break;

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

            case 0x21: // BRN (branch never)
                OpLongBranch([] { return false; });
                break;
            case 0x22: // BHI (branch if higher)
                OpLongBranch([this] { return (CC.Carry | CC.Zero) == 0; });
                break;
            case 0x23: // BLS (banch if lower or same)
                OpLongBranch([this] { return (CC.Carry | CC.Zero) != 0; });
                break;
            case 0x24: // BCC (branch if carry clear) or BHS (branch if higher or same)
                OpLongBranch([this] { return CC.Carry == 0; });
                break;
            case 0x25: // BCS (branch if carry set) or BLO (branch if lower)
                OpLongBranch([this] { return CC.Carry != 0; });
                break;
            case 0x26: // BNE (branch if not equal)
                OpLongBranch([this] { return CC.Zero == 0; });
                break;
            case 0x27: // BEQ (branch if equal)
                OpLongBranch([this] { return CC.Zero != 0; });
                break;
            case 0x28: // BVC (branch if overflow clear)
                OpLongBranch([this] { return CC.Overflow == 0; });
                break;
            case 0x29: // BVS (branch if overflow set)
                OpLongBranch([this] { return CC.Overflow != 0; });
                break;
            case 0x2A: // BPL (branch if plus)
                OpLongBranch([this] { return CC.Negative == 0; });
                break;
            case 0x2B: // BMI (brach if minus)
                OpLongBranch([this] { return CC.Negative != 0; });
                break;
            case 0x2C: // BGE (branch if greater or equal)
                OpLongBranch([this] { return (CC.Negative ^ CC.Overflow) == 0; });
                break;
            case 0x2D: // BLT (branch if less than)
                OpLongBranch([this] { return (CC.Negative ^ CC.Overflow) != 0; });
                break;
            case 0x2E: // BGT (branch if greater)
                OpLongBranch([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) == 0; });
                break;
            case 0x2F: // BLE (branch if less or equal)
                OpLongBranch([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) != 0; });
                break;

            default:
                UnhandledOp(cpuOp);
            }
            break;

        case 2:
            switch (cpuOp.opCode) {
            case 0x3F:
                OpSWI(InterruptVector::Swi3);
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
    }
};

template <>
uint16_t CpuImpl::ReadEA16<AddressingMode::Indexed>() {
    return ReadIndexedEA();
}
template <>
uint16_t CpuImpl::ReadEA16<AddressingMode::Extended>() {
    return ReadExtendedEA();
}
template <>
uint16_t CpuImpl::ReadEA16<AddressingMode::Direct>() {
    return ReadDirectEA();
}

// Specialize for Immediate mode where we don't de-ref
template <>
uint16_t CpuImpl::ReadOperandValue16<AddressingMode::Immediate>() {
    return ReadPC16();
}
template <>
uint8_t CpuImpl::ReadOperandValue8<AddressingMode::Immediate>() {
    return ReadPC8();
}

Cpu::Cpu() = default;
Cpu::~Cpu() = default;

void Cpu::Init(MemoryBus& memoryBus) {
    m_impl->Init(memoryBus);
}

void Cpu::Reset() {
    m_impl->Reset();
}

cycles_t Cpu::ExecuteInstruction(bool irqEnabled, bool firqEnabled) {
    return m_impl->ExecuteInstruction(irqEnabled, firqEnabled);
}

const CpuRegisters& Cpu::Registers() const {
    return *m_impl;
}
