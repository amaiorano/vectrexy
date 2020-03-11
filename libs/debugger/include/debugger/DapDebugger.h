#pragma once

#include "core/Base.h"
#include "core/ConsoleOutput.h"
#include "core/FileSystem.h"
#include "debugger/Breakpoints.h"
#include "debugger/DebugSymbols.h"
#include "emulator/EngineTypes.h"

#include <atomic>

class Cpu;
class Emulator;

namespace dap {
    class Session;
}

class DapDebugger {
public:
    DapDebugger();
    ~DapDebugger();

    void Init(Emulator& emulator);
    void Reset() {}

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
    Breakpoints m_breakpoints;
    DebugSymbols m_debugSymbols;
    double m_cpuCyclesLeft = 0;

    std::unique_ptr<dap::Session> m_session;
    std::atomic<bool> m_errored{false};
    std::atomic<bool> m_paused{false};

    enum class RunState {
        Running,
        Pausing,
        StepInto,
        StepOver,
        StepOut,
    };
    std::atomic<RunState> m_runState{RunState::Running};
};
