#pragma once

#include "Base.h"
#include <type_traits>
#include <array>

class MemoryBus;

// Implementation of Motorola 68A09 1.5 MHz 8-Bit Microprocessor

class Cpu
{
public:
	void Init(MemoryBus& memoryBus);
	void Reset();
	void ExecuteInstruction();

private:
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
	void InitOpCodeTables();
	const struct CpuOp& LookupCpuOp(int page, uint8_t opCode);


	// Compile-time layout validation
	static void ValidateLayout()
	{
		static_assert(std::is_standard_layout<Cpu>::value, "Can't use offsetof");
		static_assert(offsetof(Cpu, A) < offsetof(Cpu, B), "Reorder union so that A is msb and B is lsb of D");
		static_assert((offsetof(Cpu, D) - offsetof(Cpu, A)) == 0, "Reorder union so that A is msb and B is lsb of D");
	}
};
