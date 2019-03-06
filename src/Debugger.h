#pragma once

#include "Base.h"
#include "Breakpoints.h"
#include "EngineClient.h"
#include "SyncProtocol.h"
#include <map>
#include <optional>
#include <queue>
#include <string>

class MemoryBus;
class Cpu;
class Via;
class Ram;
class SyncProtocol;

class Debugger {
public:
    void Init(int argc, char** argv, MemoryBus& memoryBus, Cpu& cpu, Via& via, Ram& ram);
    void Reset();
    bool FrameUpdate(double frameTime, const Input& input, const EmuEvents& emuEvents,
                     RenderContext& renderContext, AudioContext& audioContext);

    using SymbolTable = std::multimap<uint16_t, std::string>;

private:
    void BreakIntoDebugger();
    void ResumeFromDebugger();
    void SyncInstructionHash(int numInstructionsExecutedThisFrame);

    MemoryBus* m_memoryBus = nullptr;
    Cpu* m_cpu = nullptr;
    Via* m_via = nullptr;
    Ram* m_ram = nullptr;
    bool m_breakIntoDebugger = false;
    bool m_traceEnabled = false;
    bool m_colorEnabled = false;
    std::queue<std::string> m_pendingCommands;
    std::string m_lastCommand;
    Breakpoints m_breakpoints;
    std::optional<int64_t> m_numInstructionsToExecute = {};
    SymbolTable m_symbolTable; // Address to symbol name
    cycles_t m_cpuCyclesTotal = 0;
    double m_cpuCyclesLeft = 0;
    uint32_t m_instructionHash = 0;
    SyncProtocol m_syncProtocol;
};
