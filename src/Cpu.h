#pragma once

#include "Base.h"
#include <memory>

class MemoryBus;

// Implementation of Motorola 68A09 1.5 MHz 8-Bit Microprocessor

class Cpu
{
public:
	Cpu();
	~Cpu();

	void Init(MemoryBus& memoryBus);
	void Reset(uint16_t initialPC);
	void ExecuteInstruction();

private:
	std::unique_ptr<class CpuImpl> m_impl;
};
