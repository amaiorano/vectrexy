#pragma once

#include "Base.h"
#include <type_traits>

class Cpu
{
	struct ConditionCode
	{
		uint8_t Carry : 1;
		uint8_t Overflow : 1;
		uint8_t Zero : 1;
		uint8_t Negative : 1;
		uint8_t InterruptMask : 1;
		uint8_t HalfCarry : 1;
		uint8_t FastInterruptMask : 1;
		uint8_t Entire : 1;
	};

	struct Registers
	{
		union
		{
			struct
			{
				uint8_t A;
				uint8_t B;
			};
			uint16_t D;
		};

		uint16_t PC; // program counter
		uint16_t S; // system stack pointer
		uint16_t U; // user stack pointer
		uint8_t DP; // direct page (msb of zero-page address)
		ConditionCode CC; // condition code (aka status register)
	};
	static_assert(std::is_standard_layout<Registers>::value, "Can't use offsetof");
	static_assert(offsetof(Registers, A) < offsetof(Registers, B), "Reorder union so that A is msb and B is lsb of D");
	static_assert((offsetof(Registers, D) - offsetof(Registers, A)) == 0, "Reorder union so that A is msb and B is lsb of D");

	Registers m_reg;
};

