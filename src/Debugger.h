#pragma once

#include "Base.h"
#include "Breakpoints.h"
#include <map>
#include <optional>
#include <string>

class MemoryBus;
class Cpu;

class Debugger {
public:
    void Init(MemoryBus& memoryBus, Cpu& cpu);
    bool Update(double deltaTime);

    using SymbolTable = std::multimap<uint16_t, std::string>;

private:
    MemoryBus* m_memoryBus = nullptr;
    Cpu* m_cpu = nullptr;
    bool m_breakIntoDebugger = false;
    bool m_traceEnabled = false;
    bool m_colorEnabled = false;
    uint64_t m_instructionCount = 0;
    std::string m_lastCommand;
    Breakpoints m_breakpoints;
    std::optional<int64_t> m_numInstructionsToExecute = {};
    SymbolTable m_symbolTable; // Address to symbol name
    cycles_t m_cpuCyclesTotal = 0;
    cycles_t m_cpuCyclesLeft = 0;
};
