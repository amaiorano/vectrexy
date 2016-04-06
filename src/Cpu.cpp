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
			uint8_t A;
			uint8_t B;
		};
		uint16_t D;
	};
	uint8_t DP; // direct page register (msb of zero-page address)
	ConditionCode CC; // condition code register (aka status register)

	MemoryBus* m_memoryBus = nullptr;

	// Compile-time layout validation
	static void ValidateLayout()
	{
		static_assert(std::is_standard_layout<CpuImpl>::value, "Can't use offsetof");
		static_assert(offsetof(CpuImpl, A) < offsetof(CpuImpl, B), "Reorder union so that A is msb and B is lsb of D");
		static_assert((offsetof(CpuImpl, D) - offsetof(CpuImpl, A)) == 0, "Reorder union so that A is msb and B is lsb of D");
	}

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

	void Push16(uint16_t& stackPointer, uint16_t value)
	{
		m_memoryBus->Write(--stackPointer, U8(value & 0b1111)); // Low
		m_memoryBus->Write(--stackPointer, U8(value >> 4)); // High
	}

	uint16_t Pop16(uint16_t& stackPointer)
	{
		auto high = m_memoryBus->Read(stackPointer--);
		auto low = m_memoryBus->Read(stackPointer--);
		return CombineToU16(high, low);
	}

	uint16_t Read16(uint16_t address)
	{
		auto high = m_memoryBus->Read(address++);
		auto low = m_memoryBus->Read(address);
		return CombineToU16(high, low);
	}

	uint16_t DirectEA()
	{
		// EA = DP : (PC)
		uint16_t EA = CombineToU16(DP, m_memoryBus->Read(PC++));
		return EA;
	}

	uint16_t IndexedEA()
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
		uint8_t postbyte = m_memoryBus->Read(PC++);
		bool supportsIndirect = true;

		if (postbyte & BITS(7)) // (+/- 4 bit offset),R
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
				uint8_t postbyte2 = m_memoryBus->Read(PC++);
				EA = RegisterSelect(postbyte) + S16(postbyte2);
			} break;
			case 0b1001: // (+/- 15 bit offset),R
			{
				uint8_t postbyte2 = m_memoryBus->Read(PC++);
				uint8_t postbyte3 = m_memoryBus->Read(PC++);
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
				uint8_t postbyte2 = m_memoryBus->Read(PC++);
				EA = PC + S16(postbyte2);
			} break;
			case 0b1101: // (+/- 15 bit offset),PC
			{
				uint8_t postbyte2 = m_memoryBus->Read(PC++);
				uint8_t postbyte3 = m_memoryBus->Read(PC++);
				EA = PC + CombineToS16(postbyte2, postbyte3);
			} break;
			case 0b1110:
				FAIL("Illegal");
				break;
			case 0b1111: // [address]
						 // Indirect-only
				uint8_t postbyte2 = m_memoryBus->Read(PC++);
				uint8_t postbyte3 = m_memoryBus->Read(PC++);
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

	uint16_t ExtendedEA()
	{
		// Contents of 2 bytes following opcode byte specify 16-bit effective address (always 3 byte instruction)
		// EA = (PC) : (PC + 1)
		auto msb = m_memoryBus->Read(PC++);
		auto lsb = m_memoryBus->Read(PC++);
		uint16_t EA = CombineToU16(msb, lsb);

		// @TODO: "As a special case of indexed addressing, one level of indirection may be added to extended addressing. In extended indirect,
		//         the two bytes following the postbyte of an indexed instruction contain the address of the data."
		// *** Is this handled under Indexed??

		return EA;
	}

	uint16_t Immediate16()
	{
		auto msb = m_memoryBus->Read(PC++);
		auto lsb = m_memoryBus->Read(PC++);
		uint16_t value = CombineToU16(msb, lsb);
		return value;
	}

	template <AddressingMode addressingMode>
	uint16_t ReadOperand()
	{
		assert(false && "Not implemented for addressing mode");
		return 0xFFFF;
	}

	template <>
	uint16_t ReadOperand<AddressingMode::Indexed>()
	{
		return IndexedEA();
	}
	template <>
	uint16_t ReadOperand<AddressingMode::Extended>()
	{
		return ExtendedEA();
	}
	template <>
	uint16_t ReadOperand<AddressingMode::Immediate>()
	{
		return Immediate16();
	}

	// Default template assumes operand is EA and de-refs it
	template <AddressingMode addressingMode>
	uint16_t ReadOperandValue16()
	{
		auto EA = ReadOperand<addressingMode>();
		return Read16(EA);
	}
	// Specialize for Immediate mode where we don't de-ref
	template <>
	uint16_t ReadOperandValue16<AddressingMode::Immediate>()
	{
		return ReadOperand<AddressingMode::Immediate>();
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
		uint16_t EA = ReadOperand<LookupCpuOp(page, opCode).addrMode>();
		Push16(S, PC);
		PC = EA;
	}

	void ExecuteInstruction()
	{
		auto PrintOp = [this](const CpuOp& cpuOp, int cpuOpPage)
		{
			printf("0x%04X: 0x%02X", PC - (cpuOpPage==0? 1 : 2), cpuOp.opCode);
			for (uint16_t i = 1; i < cpuOp.size; ++i)
				printf(" 0x%02X", m_memoryBus->Read(PC + i - 1));
			printf(" %s\n", cpuOp.name);
		};

		auto UnhandledOp = [this](const CpuOp& cpuOp)
		{
			(void)cpuOp;
			//printf("\tUnhandled Op!\n");
			//for (int i = 0; i < cpuOp.size - 1; ++i)
			//	printf("\tskipping 0x%02X\n", m_memoryBus->Read(PC++));
			FAIL("Unhandled Op!");
		};

		int cpuOpPage = 0;
		uint8_t opCodeByte = m_memoryBus->Read(PC++);
		if (IsOpCodePage1(opCodeByte))
		{
			cpuOpPage = 1;
			opCodeByte = m_memoryBus->Read(PC++);
		}
		else if (IsOpCodePage2(opCodeByte))
		{
			cpuOpPage = 2;
			opCodeByte = m_memoryBus->Read(PC++);
		}
		
		const CpuOp& cpuOp = LookupCpuOpRuntime(cpuOpPage, opCodeByte);

		PrintOp(cpuOp, cpuOpPage);

		assert(cpuOp.addrMode != AddressingMode::Illegal && "Illegal instruction!");
		assert(cpuOp.addrMode != AddressingMode::Variant && "Page 1/2 instruction, should have read next byte by now");


		// Compute EA from addressing mode
		// NOTE: PC currently points to the first operand byte

		uint8_t postbyte = 0;

		switch (cpuOp.addrMode)
		{
		case AddressingMode::Relative:
			// Used for branch instructions; can be either an 8 or 16 bit signed relative offset stored
			// in postbyte(s). We delay reading the offset to when evaluating the condition so that we
			// only pay for the reads (and cycles) if the branch is taken.
			break;

		case AddressingMode::Inherent:
			// Always read the "postbyte"; some instructions use it for "register addressing"
			postbyte = m_memoryBus->Read(PC++);
			break;

		case AddressingMode::Immediate:
			// 8 or 16 bit immediate value follows opcode byte (2 or 3 byte instruction)
			// 1 or 2 byte operand, depending on instruction. Here we'll just set the first byte to set the initial effective address. If the instruction
			// needs to read a 16 bit value, it can perform the extra read on PC (i.e. EA+1) and increment PC - @TODO: verify!
			//EA = PC++;
			break;

		case AddressingMode::Direct:
			break;

		case AddressingMode::Indexed:
		 break;

		case AddressingMode::Extended:
		 break;

		default:
			FAIL("Unexpected addressing mode");
		}


		switch (cpuOpPage)
		{
		case 0:
			switch (cpuOp.opCode)
			{
			case 0x9D: OpJSR<0, 0x9D>(); break;
			case 0xAD: OpJSR<0, 0xAD>(); break;
			case 0xBD: OpJSR<0, 0xBD>(); break;

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
