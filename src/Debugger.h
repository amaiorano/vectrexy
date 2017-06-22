#pragma once

#include <map>
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
    std::string m_lastCommand;
    SymbolTable m_symbolTable; // Address to symbol name
};
