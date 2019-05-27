#pragma once

#include "core/Base.h"
#include "core/CircularBuffer.h"
#include "debugger/Breakpoints.h"
#include "debugger/SyncProtocol.h"
#include "debugger/Trace.h"
#include "emulator/EngineTypes.h"
#include <map>
#include <optional>
#include <queue>
#include <string>

class Emulator;
class MemoryBus;
class Cpu;
class Via;
class Ram;
class SyncProtocol;

class Debugger {
public:
    void Init(std::shared_ptr<IEngineService>& engineService, int argc, char** argv,
              fs::path devDir, Emulator& emulator);
    void Reset();
    bool FrameUpdate(double frameTime, const Input& input, const EmuEvents& emuEvents,
                     RenderContext& renderContext, AudioContext& audioContext);

    using SymbolTable = std::multimap<uint16_t, std::string>;

private:
    void BreakIntoDebugger();
    void ResumeFromDebugger();
    void PrintOp(const Trace::InstructionTraceInfo& traceInfo);
    void PrintLastOp();
    void ExecuteFrameInstructions(double frameTime, const Input& input,
                                  RenderContext& renderContext, AudioContext& audioContext);
    cycles_t ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                AudioContext& audioContext);
    void SyncInstructionHash(int numInstructionsExecutedThisFrame);

    std::shared_ptr<IEngineService> m_engineService;
    fs::path m_devDir;
    Emulator* m_emulator = nullptr;
    MemoryBus* m_memoryBus = nullptr;
    Cpu* m_cpu = nullptr;
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
    int m_numInstructionsExecutedThisFrame = 0;
    uint32_t m_instructionHash = 0;
    SyncProtocol m_syncProtocol;

    const size_t MaxTraceInstructions = 1000'000;
    CircularBuffer<Trace::InstructionTraceInfo> m_instructionTraceBuffer{MaxTraceInstructions};
    Trace::InstructionTraceInfo* m_currTraceInfo = nullptr;
};
