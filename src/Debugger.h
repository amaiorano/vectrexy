#pragma once

#include "Breakpoints.h"
#include <map>
#include <optional>
#include <string>

class MemoryBus;
class Cpu;

class Debugger {
public:
    void Init(MemoryBus& memoryBus, Cpu& cpu);
    void Run();

    using SymbolTable = std::map<uint16_t, std::string>;

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
};
