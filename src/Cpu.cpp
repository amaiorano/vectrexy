#include "Cpu.h"
#include "MemoryBus.h"
#include "CpuOpCodes.h"

void Cpu::Init(MemoryBus& memoryBus)
{
	m_memoryBus = &memoryBus;
	InitOpCodeTables();
	Reset();
}

void Cpu::Reset()
{
	PC = 0;
}

void Cpu::ExecuteInstruction()
{
	const CpuOp& cpuOp = [&]
	{
		const uint8_t opCodeFirstByte = m_memoryBus->Read(PC++);
		if (IsOpCodePage1(opCodeFirstByte))
		{
			return LookupCpuOp(1, m_memoryBus->Read(PC++));
		}
		if (IsOpCodePage2(opCodeFirstByte))
		{
			return LookupCpuOp(2, m_memoryBus->Read(PC++));
		}
		return LookupCpuOp(0, opCodeFirstByte);
	}();

	printf("%s\n", cpuOp.name);

	for (int i = 0; i < cpuOp.size - 1; ++i)
		printf("  skipping 0x%02X\n", m_memoryBus->Read(PC++));
}

void Cpu::InitOpCodeTables()
{
	// Init m_opCodeTables to map opCode to CpuOp for fast lookups (op code indices are not all sequential)

	for (auto& opCodeTable : m_opCodeTables)
		std::fill(opCodeTable.begin(), opCodeTable.end(), nullptr);

	for (size_t i = 0; i < NumCpuOpsPage0; ++i) m_opCodeTables[0][CpuOpsPage0[i].opCode] = &CpuOpsPage0[i];
	for (size_t i = 0; i < NumCpuOpsPage1; ++i) m_opCodeTables[1][CpuOpsPage1[i].opCode] = &CpuOpsPage1[i];
	for (size_t i = 0; i < NumCpuOpsPage2; ++i) m_opCodeTables[2][CpuOpsPage2[i].opCode] = &CpuOpsPage2[i];
}

const CpuOp& Cpu::LookupCpuOp(int page, uint8_t opCode)
{
	return *m_opCodeTables[page][opCode];
}
