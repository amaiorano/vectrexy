#pragma once

#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/FileSystem.h"
#include "debugger/Breakpoints.h"
#include "debugger/CallStack.h"
#include "debugger/DebugSymbols.h"
#include "emulator/EngineTypes.h"

#include <atomic>
#include <variant>

class Cpu;
class Emulator;
class MemoryBus;

namespace dap {
    class Session;
}

class DapDebugger {
public:
    DapDebugger();
    ~DapDebugger();

    void Init(Emulator& emulator);
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
    std::atomic<bool> m_errored{false};
    std::atomic<bool> m_paused{false};

    enum class TargetState {
        Running,
        Pausing,
        StepInto,
        StepOver,
        StepOut,
        Paused,

        Invalid,
    };
    std::atomic<TargetState> m_targetState{TargetState::Running};

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
