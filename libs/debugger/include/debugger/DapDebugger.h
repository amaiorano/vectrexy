#pragma once

#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/FileSystem.h"
#include "core/TsEvent.h"
#include "core/TsQueue.h"
#include "debugger/Breakpoints.h"
#include "debugger/CallStack.h"
#include "debugger/DebugSymbols.h"
#include "emulator/EngineTypes.h"

#include <atomic>
#include <mutex>
#include <variant>

class Cpu;
class Emulator;
class MemoryBus;

namespace dap {
    class Session;
    class Writer;
} // namespace dap

class DapDebugger {
public:
    DapDebugger();
    ~DapDebugger();

    void Init(fs::path devDir, Emulator& emulator);
    void Reset();

    void OnRomLoaded(const char* file);

    bool FrameUpdate(double frameTime, const EmuEvents& emuEvents, const Input& input,
                     RenderContext& renderContext, AudioContext& audioContext);

private:
    void ParseRst(const fs::path& rstFile);
    void InitDap();
    void WaitDap();
    enum class Event { BreakpointHit, Stepped, Paused };
    void OnEvent(Event event);
    void ExecuteFrameInstructions(double frameTime, const Input& input,
                                  RenderContext& renderContext, AudioContext& audioContext);
    cycles_t ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                AudioContext& audioContext);
    bool CheckForBreakpoints();
    uint16_t PC() const;

    fs::path m_devDir;
    Emulator* m_emulator{};
    Cpu* m_cpu{};
    MemoryBus* m_memoryBus{};
    fs::path m_sourceRoot;
    Breakpoints m_internalBreakpoints;
    Breakpoints m_userBreakpoints;
    ConditionalBreakpoints m_internalConditionalBreakpoints;
    CallStack m_callStack;
    DebugSymbols m_debugSymbols;
    double m_cpuCyclesLeft = 0;

    std::unique_ptr<dap::Session> m_session;
    TsEvent m_configuredEvent;
    std::shared_ptr<dap::Writer> m_dapLog;
    std::atomic<bool> m_errored{false};
    std::mutex m_frameMutex;

    enum class DebuggerRequest {
        Pause,
        Continue,
        StepOver,
        StepInto,
        StepOut,
    };
    TsQueue<DebuggerRequest> m_requestQueue;
    TsEvent m_pausedEvent;

    struct Running {};
    struct Pausing {};
    struct StepInto {
        bool entry = true;
        SourceLocation initialSourceLocation{};
    };
    struct StepOverOrOut {
        bool isStepOver = false;
        bool entry = true;
        SourceLocation initialSourceLocation{};
        size_t initialStackDepth{};
    };
    struct StepOver : StepOverOrOut {
        StepOver() { isStepOver = true; }
    };
    struct StepOut : StepOverOrOut {
        StepOut() { isStepOver = false; }
    };
    struct FinishStepOut {
        bool entry = true;
        SourceLocation initialSourceLocation{};
    };
    struct Paused {};

    std::variant<Running, Pausing, StepInto, StepOver, StepOut, Paused, FinishStepOut> m_state{
        Running{}};
};
