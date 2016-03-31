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
	std::array<const struct CpuOp*, 256> m_opCodeTables[3]; // Lookup tables by page

	// Compile-time layout validation
	static void ValidateLayout()
	{
		static_assert(std::is_standard_layout<CpuImpl>::value, "Can't use offsetof");
		static_assert(offsetof(CpuImpl, A) < offsetof(CpuImpl, B), "Reorder union so that A is msb and B is lsb of D");
		static_assert((offsetof(CpuImpl, D) - offsetof(CpuImpl, A)) == 0, "Reorder union so that A is msb and B is lsb of D");
	}

	void InitOpCodeTables()
	{
		// Init m_opCodeTables to map opCode to CpuOp for fast lookups (op code indices are not all sequential)

		for (auto& opCodeTable : m_opCodeTables)
			std::fill(opCodeTable.begin(), opCodeTable.end(), nullptr);

		for (size_t i = 0; i < NumCpuOpsPage0; ++i) m_opCodeTables[0][CpuOpsPage0[i].opCode] = &CpuOpsPage0[i];
		for (size_t i = 0; i < NumCpuOpsPage1; ++i) m_opCodeTables[1][CpuOpsPage1[i].opCode] = &CpuOpsPage1[i];
		for (size_t i = 0; i < NumCpuOpsPage2; ++i) m_opCodeTables[2][CpuOpsPage2[i].opCode] = &CpuOpsPage2[i];
	}

	const struct CpuOp& LookupCpuOp(int page, uint8_t opCode)
	{
		return *m_opCodeTables[page][opCode];
	}

	void Init(MemoryBus& memoryBus)
	{
		m_memoryBus = &memoryBus;
		InitOpCodeTables();
		Reset();
	}

	void Reset()
	{
		X = 0;
		Y = 0;
		U = 0;
		S = 0; // @TODO: is this right? system stack pushes first value at [0xFE,0xFF]?
		PC = 0;
		DP = 0;

		CC.Value = 0;
		CC.InterruptMask = true;
		CC.FastInterruptMask = true;
	}

	void ExecuteInstruction()
	{
		auto PrintOp = [this](const CpuOp& cpuOp)
		{
			printf("0x%04X: 0x%02X", PC - 1, cpuOp.opCode);
			for (uint16_t i = 1; i < cpuOp.size; ++i)
				printf(" 0x%02X", m_memoryBus->Read(PC + i - 1));
			printf(" %s\n", cpuOp.name);
		};

		auto UnhandledOp = [this](const CpuOp& cpuOp)
		{
			printf("\tUnhandled Op!\n");
			for (int i = 0; i < cpuOp.size - 1; ++i)
				printf("\tskipping 0x%02X\n", m_memoryBus->Read(PC++));
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
		const CpuOp& cpuOp = LookupCpuOp(cpuOpPage, opCodeByte);


		PrintOp(cpuOp);

		assert(cpuOp.addrMode != AddressingMode::Illegal && "Illegal instruction!");
		assert(cpuOp.addrMode != AddressingMode::Variant && "Page 1/2 instruction, should have read next byte by now");


		// Compute EA from addressing mode
		// NOTE: PC currently points to the first operand byte

		uint16_t EA = 0; // effective address
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
			EA = PC++;
			break;

		case AddressingMode::Direct:
			// EA = DP : (PC)
			EA = CombineToU16(DP, m_memoryBus->Read(PC++));
			break;

		case AddressingMode::Indexed:
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
		} break;

		case AddressingMode::Extended:
		{
			// Contents of 2 bytes following opcode byte specify 16-bit effective address (always 3 byte instruction)
			// EA = (PC) : (PC + 1)
			auto msb = m_memoryBus->Read(PC++);
			auto lsb = m_memoryBus->Read(PC++);
			EA = CombineToU16(msb, lsb);

			// @TODO: "As a special case of indexed addressing, one level of indirection may be added to extended addressing. In extended indirect,
			//         the two bytes following the postbyte of an indexed instruction contain the address of the data."
			// *** Is this handled under Indexed??
		} break;

		default:
			FAIL("Unexpected addressing mode");
		}


		switch (cpuOpPage)
		{
		case 0:
			switch (cpuOp.opCode)
			{
			case 0x00: // NEG      
			case 0x03: // COM      
			case 0x04: // LSR      
			case 0x06: // ROR      
			case 0x07: // ASR      
			case 0x08: // LSL/ASL  
			case 0x09: // ROL      
			case 0x0A: // DEC      
			case 0x0C: // INC      
			case 0x0D: // TST      
			case 0x0E: // JMP      
			case 0x0F: // CLR      
			case 0x12: // NOP
			case 0x13: // SYNC     
			case 0x16: // LBRA     
			case 0x17: // LBSR     
			case 0x19: // DAA      
			case 0x1A: // ORCC     
			case 0x1C: // ANDCC    
			case 0x1D: // SEX      
			case 0x1E: // EXG      
			case 0x1F: // TFR      
			case 0x20: // BRA      
			case 0x21: // BRN      
			case 0x22: // BHI      
			case 0x23: // BLS      
			case 0x24: // BHS/BCC  
			case 0x25: // BLO/BCS  
			case 0x26: // BNE      
			case 0x27: // BEQ      
			case 0x28: // BVC      
			case 0x29: // BVS      
			case 0x2A: // BPL      
			case 0x2B: // BMI      
			case 0x2C: // BGE      
			case 0x2D: // BLT      
			case 0x2E: // BGT      
			case 0x2F: // BLE      
			case 0x30: // LEAX     
			case 0x31: // LEAY     
			case 0x32: // LEAS     
			case 0x33: // LEAU     
			case 0x34: // PSHS     
					   //TODO: assert that S is not selected in postbyte (assembler would not have allowed it)
			case 0x35: // PULS     
			case 0x36: // PSHU    
					   //TODO: assert that U is not selected in postbyte (assembler would not have allowed it)
			case 0x37: // PULU     
			case 0x39: // RTS      
			case 0x3A: // ABX      
			case 0x3B: // RTI      
			case 0x3C: // CWAI     
			case 0x3D: // MUL      
			case 0x3E: // RESET*   
			case 0x3F: // SWI      
			case 0x40: // NEGA     
			case 0x43: // COMA     
			case 0x44: // LSRA     
			case 0x46: // RORA     
			case 0x47: // ASRA     
			case 0x48: // LSLA/AS  
			case 0x49: // ROLA     
			case 0x4A: // DECA     
			case 0x4C: // INCA     
			case 0x4D: // TSTA     
			case 0x4F: // CLRA     
			case 0x50: // NEGB     
			case 0x53: // COMB     
			case 0x54: // LSRB     
			case 0x56: // RORB     
			case 0x57: // ASRB     
			case 0x58: // LSLB/AS  
			case 0x59: // ROLB     
			case 0x5A: // DECB     
			case 0x5C: // INCB     
			case 0x5D: // TSTB     
			case 0x5F: // CLRB     
			case 0x60: // NEG      
			case 0x63: // COM      
			case 0x64: // LSR      
			case 0x66: // ROR      
			case 0x67: // ASR      
			case 0x68: // LSL/ASL  
			case 0x69: // ROL      
			case 0x6A: // DEC      
			case 0x6C: // INC      
			case 0x6D: // TST      
			case 0x6E: // JMP      
			case 0x6F: // CLR      
			case 0x70: // NEG      
			case 0x73: // COM      
			case 0x74: // LSR      
			case 0x76: // ROR      
			case 0x77: // ASR      
			case 0x78: // LSL/ASL  
			case 0x79: // ROL      
			case 0x7A: // DEC      
			case 0x7C: // INC      
			case 0x7D: // TST      
			case 0x7E: // JMP      
			case 0x7F: // CLR      
			case 0x80: // SUBA     
			case 0x81: // CMPA     
			case 0x82: // SBCA     
			case 0x83: // SUBD     
			case 0x84: // ANDA     
			case 0x85: // BITA     
			case 0x86: // LDA      
			case 0x88: // EORA     
			case 0x89: // ADCA     
			case 0x8A: // ORA      
			case 0x8B: // ADDA     
			case 0x8C: // CMPX     
			case 0x8D: // BSR      
			case 0x8E: // LDX      
			case 0x90: // SUBA     
			case 0x91: // CMPA     
			case 0x92: // SBCA     
			case 0x93: // SUBD     
			case 0x94: // ANDA     
			case 0x95: // BITA     
			case 0x96: // LDA      
			case 0x97: // STA      
			case 0x98: // EORA     
			case 0x99: // ADCA     
			case 0x9A: // ORA      
			case 0x9B: // ADDA     
			case 0x9C: // CMPX     

			case 0x9D: // JSR
			case 0xAD: // JSR      
			case 0xBD: // JSR      
					   // Push return address onto system stack
				--S;
				m_memoryBus->Write(S, U8(PC >> 4));
				--S;
				m_memoryBus->Write(S, U8(PC & 0b1111));
				// Jump to EA
				PC = EA;
				break;
			case 0x9E: // LDX      
			case 0x9F: // STX      
			case 0xA0: // SUBA     
			case 0xA1: // CMPA     
			case 0xA2: // SBCA     
			case 0xA3: // SUBD     
			case 0xA4: // ANDA     
			case 0xA5: // BITA     
			case 0xA6: // LDA      
			case 0xA7: // STA      
			case 0xA8: // EORA     
			case 0xA9: // ADCA     
			case 0xAA: // ORA      
			case 0xAB: // ADDA     
			case 0xAC: // CMPX     
			case 0xAE: // LDX      
			case 0xAF: // STX      
			case 0xB0: // SUBA     
			case 0xB1: // CMPA     
			case 0xB2: // SBCA     
			case 0xB3: // SUBD     
			case 0xB4: // ANDA     
			case 0xB5: // BITA     
			case 0xB6: // LDA      
			case 0xB7: // STA      
			case 0xB8: // EORA     
			case 0xB9: // ADCA     
			case 0xBA: // ORA      
			case 0xBB: // ADDA     
			case 0xBC: // CMPX     
			case 0xBE: // LDX      
			case 0xBF: // STX      
			case 0xC0: // SUBB     
			case 0xC1: // CMPB     
			case 0xC2: // SBCB     
			case 0xC3: // ADDD     
			case 0xC4: // ANDB     
			case 0xC5: // BITB     
			case 0xC6: // LDB      
			case 0xC8: // EORB     
			case 0xC9: // ADCB     
			case 0xCA: // ORB      
			case 0xCB: // ADDB     
			case 0xCC: // LDD      
			case 0xCE: // LDU      
			case 0xD0: // SUBB     
			case 0xD1: // CMPB     
			case 0xD2: // SBCB     
			case 0xD3: // ADDD     
			case 0xD4: // ANDB     
			case 0xD5: // BITB     
			case 0xD6: // LDB      
			case 0xD7: // STB      
			case 0xD8: // EORB     
			case 0xD9: // ADCB     
			case 0xDA: // ORB      
			case 0xDB: // ADDB     
			case 0xDC: // LDD      
			case 0xDD: // STD      
			case 0xDE: // LDU      
			case 0xDF: // STU      
			case 0xE0: // SUBB     
			case 0xE1: // CMPB     
			case 0xE2: // SBCB     
			case 0xE3: // ADDD     
			case 0xE4: // ANDB     
			case 0xE5: // BITB     
			case 0xE6: // LDB      
			case 0xE7: // STB      
			case 0xE8: // EORB     
			case 0xE9: // ADCB     
			case 0xEA: // ORB      
			case 0xEB: // ADDB     
			case 0xEC: // LDD      
			case 0xED: // STD      
			case 0xEE: // LDU      
			case 0xEF: // STU      
			case 0xF0: // SUBB     
			case 0xF1: // CMPB     
			case 0xF2: // SBCB     
			case 0xF3: // ADDD     
			case 0xF4: // ANDB     
			case 0xF5: // BITB     
			case 0xF6: // LDB      
			case 0xF7: // STB      
			case 0xF8: // EORB     
			case 0xF9: // ADCB     
			case 0xFA: // ORB      
			case 0xFB: // ADDB     
			case 0xFC: // LDD      
			case 0xFD: // STD      
			case 0xFE: // LDU      
			case 0xFF: // STU      
				break;

			default:
				UnhandledOp(cpuOp);
			}
			break;

		case 1:
			switch (cpuOp.opCode)
			{
			case 0x00:
				break;
			default:
				UnhandledOp(cpuOp);
			}
			break;

		case 2:
			switch (cpuOp.opCode)
			{
			case 0x00:
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

void Cpu::Reset()
{
	m_impl->Reset();
}

void Cpu::ExecuteInstruction()
{
	m_impl->ExecuteInstruction();
}
