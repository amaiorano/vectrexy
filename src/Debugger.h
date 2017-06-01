#pragma once

class MemoryBus;
class Cpu;

class Debugger {
public:
    void Init(MemoryBus& memoryBus, Cpu& cpu);
    void Run();

private:
    MemoryBus* m_memoryBus = nullptr;
    Cpu* m_cpu = nullptr;
    bool m_breakIntoDebugger = false;
};
