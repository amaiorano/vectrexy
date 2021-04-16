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
    struct Variable;
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

    dap::Variable CreateDapVariable(std::shared_ptr<Variable> var, uint16_t varAddress);
    std::vector<dap::Variable> CreateChildDapVariables(std::shared_ptr<Variable> parentVar,
                                                       uint16_t parentVarAddress);

    // Used to temporarily store variables that may be expanded (e.g. pointers, structs, unions).
    // When these variables are added, along with their address, we return a unique id that can be
    // used to retrieve the pair if the variable is expanded.
    class DynamicVariables {
    public:
        struct VariableData {
            std::shared_ptr<Variable> var;
            uint16_t varAddress{};
        };

        // Adds a variable, and returns a unique id
        int AddVariable(std::shared_ptr<Variable> variable, uint16_t varAddress) {
            int id = m_idCounter;
            m_idToVariable.try_emplace(m_idCounter++,
                                       VariableData{std::move(variable), varAddress});
            return id;
        }

        // Returns the variable by input id, removing it from the set
        VariableData GetAndRemoveVariableById(int id) {
            auto iter = m_idToVariable.find(id);
            ASSERT(iter != m_idToVariable.end());
            auto result = std::move(iter->second);
            m_idToVariable.erase(iter);
            return result;
        }

        void Clear() {
            m_idCounter = 0;
            m_idToVariable.clear();
        }

    private:
        int m_idCounter{};
        std::unordered_map<int, VariableData> m_idToVariable;
    } m_dynamicVariables;

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
