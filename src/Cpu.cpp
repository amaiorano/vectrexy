#include "Cpu.h"
#include "MemoryBus.h"
#include "CpuOpCodes.h"
#include <type_traits>
#include <array>

namespace
{
	template <typename T>
	constexpr int16_t S16(T v) { return static_cast<int16_t>(v); }

	template <typename T>
	constexpr uint16_t U16(T v) { return static_cast<uint16_t>(v); }

	template <typename T>
	constexpr uint8_t U8(T v) { return static_cast<uint8_t>(v); }

	constexpr uint16_t CombineToU16(uint8_t msb, uint8_t lsb) { return U16(msb) << 8 | U16(lsb); }

	constexpr int16_t CombineToS16(uint8_t msb, uint8_t lsb) { return static_cast<int16_t>(CombineToU16(msb, lsb)); }
}

class CpuImpl
{
public:
	struct ConditionCode
	{
		union
		{
			struct
			{
				uint8_t Carry : 1;
				uint8_t Overflow : 1; // V
				uint8_t Zero : 1;
				uint8_t Negative : 1;
				uint8_t InterruptMask : 1; // IRQ
				uint8_t HalfCarry : 1;
				uint8_t FastInterruptMask : 1; // FIRQ
				uint8_t Entire : 1;
			};
			uint8_t Value; // Use only to reset to 0 or serialize
		};
	};
	static_assert(sizeof(ConditionCode) == 1, "");

	// Registers
	uint16_t X; // index register
	uint16_t Y; // index register
	uint16_t U; // user stack pointer
	uint16_t S; // hardware stack pointer
	uint16_t PC; // program counter
	union // accumulators
	{
		struct
		{
		#if ENDIANESS_LITTLE
			uint8_t B;
			uint8_t A;
		#else
			uint8_t A;
			uint8_t B;
		#endif
		};
		uint16_t D;
	};
	uint8_t DP; // direct page register (msb of zero-page address)
	ConditionCode CC; // condition code register (aka status register)

	MemoryBus* m_memoryBus = nullptr;

	void Init(MemoryBus& memoryBus)
	{
		m_memoryBus = &memoryBus;
		Reset(0);
	}

	void Reset(uint16_t initialPC)
	{
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

	uint8_t Read8(uint16_t address)
	{
		return m_memoryBus->Read(address);
	}

	uint16_t Read16(uint16_t address)
	{
		auto high = m_memoryBus->Read(address++);
		auto low = m_memoryBus->Read(address);
		return CombineToU16(high, low);
	}

	uint8_t ReadPC8()
	{
		return Read8(PC++);
	}
	uint16_t ReadPC16()
	{
		uint16_t value = Read16(PC);
		PC += 2;
		return value;
	}

	void Push16(uint16_t& stackPointer, uint16_t value)
	{
		m_memoryBus->Write(--stackPointer, U8(value & 0b11111111)); // Low
		m_memoryBus->Write(--stackPointer, U8(value >> 8)); // High
	}

	uint16_t Pop16(uint16_t& stackPointer)
	{
		auto high = m_memoryBus->Read(stackPointer++);
		auto low = m_memoryBus->Read(stackPointer++);
		return CombineToU16(high, low);
	}

	uint16_t ReadDirectEA()
	{
		// EA = DP : (PC)
		uint16_t EA = CombineToU16(DP, ReadPC8());
		return EA;
	}

	uint16_t ReadIndexedEA()
	{
		// In all indexed addressing one of the pointer registers (X, Y, U, S and sometimes PC) is used in a calculation of the EA.
		// The postbyte specifies type and variation of addressing mode as well as pointer registers to be used.
		//@TODO: add extra cycles

		auto RegisterSelect = [this](uint8_t postbyte) -> uint16_t
		{
			switch ((postbyte >> 5) & 0b11)
			{
			case 0b00: return X;
			case 0b01: return Y;
			case 0b10: return U;
			case 0b11: return S;
			}
			return 0xFF; // Impossible, but MSVC complains about "not all control paths return value"
		};

		uint16_t EA = 0;
		uint8_t postbyte = ReadPC8();
		bool supportsIndirect = true;

		if ((postbyte & BITS(7)) == 0) // (+/- 4 bit offset),R
		{
			// postbyte is a 5 bit two's complement number we convert to 8 bit.
			// So if bit 4 is set (sign bit), we extend the sign bit by turning on bits 6,7,8;
			uint8_t offset = postbyte & 0b0000'1111;
			if (postbyte & BITS(4))
				offset |= 0b1110'0000;
			EA = RegisterSelect(postbyte) + S16(offset);
			supportsIndirect = false;
		}
		else
		{
			switch (postbyte & 0b1111)
			{
			case 0b0000: // ,R+
				EA = RegisterSelect(postbyte) + 1;
				supportsIndirect = false;
				break;
			case 0b0001: // ,R++
				EA = RegisterSelect(postbyte) + 2;
				break;
			case 0b0010: // ,-R
				EA = RegisterSelect(postbyte) - 1;
				supportsIndirect = false;
				break;
			case 0b0011: // ,--R
				EA = RegisterSelect(postbyte) - 2;
				break;
			case 0b0100: // ,R
				EA = RegisterSelect(postbyte);
				break;
			case 0b0101: // (+/- B),R
				EA = RegisterSelect(postbyte) + S16(B);
				break;
			case 0b0110: // (+/- A),R
				EA = RegisterSelect(postbyte) + S16(A);
				break;
			case 0b0111:
				FAIL("Illegal");
				break;
			case 0b1000: // (+/- 7 bit offset),R
			{
				uint8_t postbyte2 = ReadPC8();
				EA = RegisterSelect(postbyte) + S16(postbyte2);
			} break;
			case 0b1001: // (+/- 15 bit offset),R
			{
				uint8_t postbyte2 = ReadPC8();
				uint8_t postbyte3 = ReadPC8();
				EA = RegisterSelect(postbyte) + CombineToS16(postbyte2, postbyte3);
			} break;
			case 0b1010:
				FAIL("Illegal");
				break;
			case 0b1011: // (+/- D),R
				EA = RegisterSelect(postbyte) + S16(D);
				break;
			case 0b1100: // (+/- 7 bit offset),PC
			{
				uint8_t postbyte2 = ReadPC8();
				EA = PC + S16(postbyte2);
			} break;
			case 0b1101: // (+/- 15 bit offset),PC
			{
				uint8_t postbyte2 = ReadPC8();
				uint8_t postbyte3 = ReadPC8();
				EA = PC + CombineToS16(postbyte2, postbyte3);
			} break;
			case 0b1110:
				FAIL("Illegal");
				break;
			case 0b1111: // [address]
						 // Indirect-only
				uint8_t postbyte2 = ReadPC8();
				uint8_t postbyte3 = ReadPC8();
				EA = CombineToS16(postbyte2, postbyte3);
				break;
			}
		}

		if (supportsIndirect && (postbyte & BITS(4)))
		{
			uint8_t msb = m_memoryBus->Read(EA);
			uint8_t lsb = m_memoryBus->Read(EA + 1);
			EA = CombineToU16(lsb, msb);
		}

		return EA;
	}

	uint16_t ReadExtendedEA()
	{
		// Contents of 2 bytes following opcode byte specify 16-bit effective address (always 3 byte instruction)
		// EA = (PC) : (PC + 1)
		auto msb = ReadPC8();
		auto lsb = ReadPC8();
		uint16_t EA = CombineToU16(msb, lsb);

		// @TODO: "As a special case of indexed addressing, one level of indirection may be added to extended addressing. In extended indirect,
		//         the two bytes following the postbyte of an indexed instruction contain the address of the data."
		// *** Is this handled under Indexed??

		return EA;
	}


	template <AddressingMode addressingMode>
	uint16_t ReadOperand16()
	{
		assert(false && "Not implemented for addressing mode");
		return 0xFFFF;
	}

	template <>
	uint16_t ReadOperand16<AddressingMode::Indexed>()
	{
		return ReadIndexedEA();
	}
	template <>
	uint16_t ReadOperand16<AddressingMode::Extended>()
	{
		return ReadExtendedEA();
	}
	template <>
	uint16_t ReadOperand16<AddressingMode::Direct>()
	{
		return ReadDirectEA();
	}


	// Default template assumes operand is EA and de-refs it
	template <AddressingMode addressingMode>
	uint16_t ReadOperandValue16()
	{
		auto EA = ReadOperand16<addressingMode>();
		return Read16(EA);
	}
	// Specialize for Immediate mode where we don't de-ref
	template <>
	uint16_t ReadOperandValue16<AddressingMode::Immediate>()
	{
		return ReadPC16();
	}

	template <AddressingMode addressingMode>
	uint8_t ReadOperandValue8()
	{
		auto EA = ReadOperand16<addressingMode>();
		return Read8(EA);
	}
	template <>
	uint8_t ReadOperandValue8<AddressingMode::Immediate>()
	{
		return ReadPC8();
	}


	int8_t ReadRelativeOffset8()
	{
		return static_cast<int8_t>(ReadPC8());
	}

	int16_t ReadRelativeOffset16()
	{
		return static_cast<int16_t>(ReadPC16());
	}


	template <int page, uint8_t opCode>
	void OpLD(uint8_t& targetReg)
	{
		uint8_t value = ReadOperandValue8<LookupCpuOp(page, opCode).addrMode>();
		CC.Negative = (value & BITS(7)) != 0;
		CC.Zero = (value == 0);
		CC.Overflow = 0;
		targetReg = value;
	}

	template <int page, uint8_t opCode>
	void OpLD(uint16_t& targetReg)
	{
		uint16_t value = ReadOperandValue16<LookupCpuOp(page, opCode).addrMode>();
		CC.Negative = (value & BITS(15)) != 0;
		CC.Zero = (value == 0);
		CC.Overflow = 0;
		targetReg = value;
	}

	template <int page, uint8_t opCode>
	void OpJSR()
	{
		uint16_t EA = ReadOperand16<LookupCpuOp(page, opCode).addrMode>();
		Push16(S, PC);
		PC = EA;
	}

	template <int page, uint8_t opCode>
	void OpCLR()
	{
		uint16_t EA = ReadOperand16<LookupCpuOp(page, opCode).addrMode>();
		m_memoryBus->Write(EA, 0);
		CC.Negative = 0;
		CC.Zero = 1;
		CC.Overflow = 0;
		CC.Carry = 0;
	}

	template <typename CondFunc>
	void Branch(CondFunc condFunc)
	{
		int8_t offset = ReadRelativeOffset8();
		if (condFunc())
			PC += offset;
	}

	void ExecuteInstruction()
	{
		auto PrintOp = [this](const CpuOp& cpuOp, int cpuOpPage)
		{
			printf("0x%04X: %9s 0x%02X", PC - (cpuOpPage==0? 1 : 2), cpuOp.name, cpuOp.opCode);
			for (uint16_t i = 1; i < cpuOp.size; ++i)
				printf(" 0x%02X", m_memoryBus->Read(PC + i - 1));
			printf("\n");
		};

		auto UnhandledOp = [this](const CpuOp& cpuOp)
		{
			(void)cpuOp;
			FAIL("Unhandled Op!");
		};

		int cpuOpPage = 0;
		uint8_t opCodeByte = ReadPC8();
		if (IsOpCodePage1(opCodeByte))
		{
			cpuOpPage = 1;
			opCodeByte = ReadPC8();
		}
		else if (IsOpCodePage2(opCodeByte))
		{
			cpuOpPage = 2;
			opCodeByte = ReadPC8();
		}
		
		const CpuOp& cpuOp = LookupCpuOpRuntime(cpuOpPage, opCodeByte);

		PrintOp(cpuOp, cpuOpPage);

		assert(cpuOp.addrMode != AddressingMode::Illegal && "Illegal instruction!");
		assert(cpuOp.addrMode != AddressingMode::Variant && "Page 1/2 instruction, should have read next byte by now");

		switch (cpuOpPage)
		{
		case 0:
			switch (cpuOp.opCode)
			{
			case 0x9D: OpJSR<0, 0x9D>(); break;
			case 0xAD: OpJSR<0, 0xAD>(); break;
			case 0xBD: OpJSR<0, 0xBD>(); break;

			// 8-bit LD
			case 0x86: OpLD<0, 0x86>(A); break;
			case 0x96: OpLD<0, 0x96>(A); break;
			case 0xA6: OpLD<0, 0xA6>(A); break;
			case 0xB6: OpLD<0, 0xB6>(A); break;
			case 0xC6: OpLD<0, 0xC6>(B); break;
			case 0xD6: OpLD<0, 0xD6>(B); break;
			case 0xE6: OpLD<0, 0xE6>(B); break;
			case 0xF6: OpLD<0, 0xF6>(B); break;
			// 16-bit LD
			case 0x8E: OpLD<0, 0x8E>(X); break;
			case 0x9E: OpLD<0, 0x9E>(X); break;
			case 0xAE: OpLD<0, 0xAE>(X); break;
			case 0xBE: OpLD<0, 0xBE>(X); break;
			case 0xCC: OpLD<0, 0xCC>(D); break;
			case 0xDC: OpLD<0, 0xDC>(D); break;
			case 0xEC: OpLD<0, 0xEC>(D); break;
			case 0xFC: OpLD<0, 0xFC>(D); break;
			case 0xCE: OpLD<0, 0xCE>(U); break;
			case 0xDE: OpLD<0, 0xDE>(U); break;
			case 0xEE: OpLD<0, 0xEE>(U); break;
			case 0xFE: OpLD<0, 0xFE>(U); break;

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

			case 0x24: Branch([this] { return CC.Carry == 0; }); break; // BCC (branch if carry clear) or BHS (branch if higher or same)
			case 0x25: Branch([this] { return CC.Carry != 0; }); break; // BCS (branch if carry set) or BLO (branch if lower)
			case 0x27: Branch([this] { return CC.Zero != 0; }); break; // BEQ (branch if equal)
			case 0x2C: Branch([this] { return (CC.Negative ^ CC.Overflow) == 0; }); break; // BGE (branch if greater or equal)
			case 0x2E: Branch([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) == 0; }); break; // BGT (branch if greater)
			case 0x22: Branch([this] { return (CC.Carry | CC.Overflow) == 0; }); break; // BHI (branch if higher)
			case 0x2F: Branch([this] { return (CC.Zero | (CC.Negative ^ CC.Overflow)) != 0; }); break; // BLE (branch if less or equal)
			case 0x23: Branch([this] { return (CC.Carry | CC.Overflow) != 0; }); break; // BLS (banch if lower or same)
			case 0x2D: Branch([this] { return (CC.Negative ^ CC.Overflow) != 0; }); break; // BLT (branch if less than)
			case 0x2B: Branch([this] { return CC.Negative != 0; }); break; // BMI (brach if minus)
			case 0x26: Branch([this] { return CC.Zero == 0; }); break; // BNE (branch if not equal)
			case 0x2A: Branch([this] { return CC.Negative == 0; }); break; // BPL (branch if plus)
			case 0x20: Branch([this] { return true; }); break; // BRA (branch always)
			case 0x21: Branch([this] { return false; }); break; // BRN (branch never)
			case 0x28: Branch([this] { return CC.Overflow == 0; }); break; // BVC (branch if overflow clear)
			case 0x29: Branch([this] { return CC.Overflow != 0; }); break; // BVS (branch if overflow set)

			case 0x1F: // TFR (transfer register to register)
			{
				uint8_t postbyte = ReadPC8();
				assert(!!(postbyte & BITS(3)) == !!(postbyte & BITS(7))); // 8-bit to 8-bit or 16-bit to 16-bit only

				uint8_t src = (postbyte >> 4) & 0b111;
				uint8_t dst = postbyte & 0b111;

				if (postbyte & BITS(3))
				{
					assert(src < 4 && dst < 4); // Only first 4 are valid 8-bit register indices
					uint8_t* const reg[]{ &A, &B, &CC.Value, &DP };
					*reg[dst] = *reg[src];
				}
				else
				{
					assert(src < 6 && dst < 6); // Only first 6 are valid 16-bit register indices
					uint16_t* const reg[]{ &D, &X, &Y, &U, &S, &PC };
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
			case 0x0F: OpCLR<0, 0x0F>(); break;
			case 0x6F: OpCLR<0, 0x6F>(); break;
			case 0x7F: OpCLR<0, 0x7F>(); break;
			

			default:
				UnhandledOp(cpuOp);
			}
			break;

		case 1:
			switch (cpuOp.opCode)
			{
			case 0x8E: OpLD<1, 0x8E>(Y); break;
			case 0x9E: OpLD<1, 0x9E>(Y); break;
			case 0xAE: OpLD<1, 0xAE>(Y); break;
			case 0xBE: OpLD<1, 0xBE>(Y); break;
			case 0xCE: OpLD<1, 0xCE>(S); break;
			case 0xDE: OpLD<1, 0xDE>(S); break;
			case 0xEE: OpLD<1, 0xEE>(S); break;
			case 0xFE: OpLD<1, 0xFE>(S); break;

			default:
				UnhandledOp(cpuOp);
			}
			break;

		case 2:
			switch (cpuOp.opCode)
			{
			case 0x00:
				UnhandledOp(cpuOp);
				break;
			default:
				UnhandledOp(cpuOp);
			}
			break;
		}
	}
};

Cpu::Cpu()
	: m_impl(std::make_unique<CpuImpl>())
{
}

Cpu::~Cpu()
{
}

void Cpu::Init(MemoryBus& memoryBus)
{
	m_impl->Init(memoryBus);
}

void Cpu::Reset(uint16_t initialPC)
{
	m_impl->Reset(initialPC);
}

void Cpu::ExecuteInstruction()
{
	m_impl->ExecuteInstruction();
}
