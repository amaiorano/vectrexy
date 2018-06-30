#pragma once

#include "Base.h"
#include "Breakpoints.h"
#include "EngineClient.h"
#include <map>
#include <optional>
#include <queue>
#include <string>

class MemoryBus;
class Cpu;
class Via;
class SyncProtocol;

class Debugger {
public:
    void Init(MemoryBus& memoryBus, Cpu& cpu, Via& via);
    void Reset();
    bool FrameUpdate(double frameTime, const Input& input, const EmuEvents& emuEvents,
                     RenderContext& renderContext, AudioContext& audioContext,
                     SyncProtocol& syncProtocol);

    using SymbolTable = std::multimap<uint16_t, std::string>;

private:
    void BreakIntoDebugger();
    void ResumeFromDebugger();
    void SyncInstructionHash(SyncProtocol& syncProtocol, int numInstructionsExecutedThisFrame);

    MemoryBus* m_memoryBus = nullptr;
    Cpu* m_cpu = nullptr;
    Via* m_via = nullptr;
    bool m_breakIntoDebugger = false;
    bool m_traceEnabled = false;
    bool m_colorEnabled = false;
    std::queue<std::string> m_pendingCommands;
    uint64_t m_instructionCount = 0;
    std::string m_lastCommand;
    Breakpoints m_breakpoints;
    std::optional<int64_t> m_numInstructionsToExecute = {};
    SymbolTable m_symbolTable; // Address to symbol name
    cycles_t m_cpuCyclesTotal = 0;
    double m_cpuCyclesLeft = 0;
    uint32_t m_instructionHash = 0;
};
