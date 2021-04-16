#pragma once

#include "core/Base.h"
#include "core/CircularBuffer.h"
#include "debugger/Breakpoints.h"
#include "debugger/CallStack.h"
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
    void Init(const std::vector<std::string_view>& args,
              std::shared_ptr<IEngineService>& engineService, fs::path devDir, Emulator& emulator);
    void Reset();
    bool FrameUpdate(double frameTime, const EmuEvents& emuEvents, const Input& input,
                     RenderContext& renderContext, AudioContext& audioContext);

    using SymbolTable = std::multimap<uint16_t, std::string>;

private:
    void BreakIntoDebugger(bool switchFocus = true);
    void ResumeFromDebugger(bool switchFocus = true);
    void PrintOp(const Trace::InstructionTraceInfo& traceInfo);
    void PrintLastOp();
    void PrintCallStack();
    void CheckForBreakpoints();
    void PostOpUpdateCallstack(const CpuRegisters& preOpRegisters);
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
    ConditionalBreakpoints m_conditionalBreakpoints;
    CallStack m_callStack;
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
