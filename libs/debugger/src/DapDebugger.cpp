#include "debugger/DapDebugger.h"
#include "core/StdUtil.h"
#include "core/StringUtil.h"
#include "debugger/DebuggerUtil.h"
#include "debugger/RstParser.h"
#include "emulator/Cpu.h"
#include "emulator/Emulator.h"

#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>

// TODO: move to utility header
// Appends, via move, values from source to target
template <typename T, typename U>
T& container_move_append(T& target, U& source) {
    for (auto& s : source) {
        target.push_back(std::move(s));
    }
    return target;
}

namespace {
    // We only have one main thread, so use a constant id
    const dap::integer DAP_THREAD_ID = 100;

    // Constructs filesystem::path from the input string.
    // The only difference with the standard path constructor is that on Windows, it makes sure to
    // lower the case of the driver letter (if any). This allows for path comparisons to work as
    // expected, so that "C:\a\b\c" == "c:\a\b\c".
    fs::path MakePath(const std::string& s) {
        auto p = fs::path{s};
#if defined(PLATFORM_WINDOWS)
        if (p.has_root_name()) {
            auto rn = StringUtil::ToLower(p.root_name().string());
            p = rn + s.substr(rn.size());
        }
#endif
        return p;
    };

    struct VariableRef {
        enum class Type : uint8_t { CallStackIndex = 1, ParentVariableId };

        VariableRef(Type type, int value)
            : m_type(static_cast<int>(type))
            , m_value(value) {}

        VariableRef(int value)
            : m_type(0)
            , m_value(0) {
            std::memcpy(this, &value, sizeof(value));
        }

        int AsInt() const {
            int result{};
            std::memcpy(&result, this, sizeof(result));
            return result;
        }

        Type GetType() const { return static_cast<Type>(m_type); }
        int GetValue() const { return m_value; }

    private:
        int m_type : 8;
        int m_value : 24;
    };
    static_assert(sizeof(VariableRef) == sizeof(int));

} // namespace

DapDebugger::DapDebugger() = default;
DapDebugger::~DapDebugger() = default;

void DapDebugger::Init(fs::path devDir, Emulator& emulator) {
    m_devDir = std::move(devDir);
    m_emulator = &emulator;
    m_cpu = &m_emulator->GetCpu();
    m_memoryBus = &emulator.GetMemoryBus();
    InitDap();
}

void DapDebugger::Reset() {
    m_callStack.Clear();
}

void DapDebugger::OnRomLoaded(const char* file) {
    m_sourceRoot = MakePath(file).remove_filename().generic_string();

    // Collect .rst files in the same folder as the rom file
    std::vector<fs::path> rstFiles;
    for (auto& d : fs::directory_iterator(m_sourceRoot)) {
        if (d.path().extension() == ".rst") {
            rstFiles.push_back(d.path());
        }
    }

    for (auto& rstFile : rstFiles) {
        ParseRst(rstFile);
    }

    WaitDap();
}

bool DapDebugger::FrameUpdate(double frameTime, const EmuEvents& emuEvents, const Input& input,
                              RenderContext& renderContext, AudioContext& audioContext) {
    (void)emuEvents;

    if (m_errored)
        return false;

    ExecuteFrameInstructions(frameTime, input, renderContext, audioContext);

    return true;
}

void DapDebugger::ParseRst(const fs::path& rstFile) {
    RstParser parser{m_debugSymbols};
    parser.Parse(rstFile);
}

void DapDebugger::InitDap() {
    m_session = dap::Session::create();
    m_dapLog = dap::file((m_devDir / "dap_log.txt").string().c_str());

    // Handle errors reported by the Session. These errors include protocol
    // parsing errors and receiving messages with no handler.
    m_session->onError([&](const char* msg) {
        Printf("dap::Session error: %s\n", msg);
        if (m_dapLog) {
            dap::writef(m_dapLog, "dap::Session error: %s\n", msg);
            m_dapLog->close();
        }
        m_errored = true;
    });

    // The Initialize request is the first message sent from the client and
    // the response reports debugger capabilities.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Initialize
    m_session->registerHandler([](const dap::InitializeRequest&) {
        dap::InitializeResponse response;
        response.supportsConfigurationDoneRequest = true;
        return response;
    });

    // When the Initialize response has been sent, we need to send the initialized
    // event.
    // We use the registerSentHandler() to ensure the event is sent *after* the
    // initialize response.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Events_Initialized
    m_session->registerSentHandler([&](const dap::ResponseOrError<dap::InitializeResponse>&) {
        m_session->send(dap::InitializedEvent());
    });

    // The Threads request queries the debugger's list of active threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Threads
    m_session->registerHandler([&](const dap::ThreadsRequest&) {
        dap::ThreadsResponse response;
        dap::Thread thread;
        thread.id = DAP_THREAD_ID;
        thread.name = "MainThread";
        response.threads.push_back(thread);
        return response;
    });

    // The StackTrace request reports the stack frames (call stack) for a given thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StackTrace
    m_session->registerHandler([&](const dap::StackTraceRequest& request)
                                   -> dap::ResponseOrError<dap::StackTraceResponse> {
        if (request.threadId != DAP_THREAD_ID) {
            return dap::Error("Unknown threadId '%d'", int(request.threadId));
        }

        auto lock = std::lock_guard{m_frameMutex};

        dap::StackTraceResponse response;

        size_t i = 0;
        const auto& frames = m_callStack.Frames();
        for (auto iter = frames.rbegin(); iter != frames.rend(); ++iter, ++i) {

            dap::StackFrame frame;
            frame.column = 1;
            frame.id = static_cast<int>(frames.size() - i - 1); // Store callstack index

            // Show the function name, if available, or its address otherwise
            if (auto* symbol = m_debugSymbols.GetSymbolByAddress(iter->frameAddress)) {
                frame.name = symbol->name;
            } else {
                frame.name = FormattedString<>("0x%04x()", iter->frameAddress);
            }

            auto currAddress = (i == 0) ? m_cpu->Registers().PC : (iter - 1)->calleeAddress;

            if (auto* location = m_debugSymbols.GetSourceLocation(currAddress)) {
                dap::Source source;
                source.name = fs::path{location->file}.filename().string();
                source.path = (m_sourceRoot / location->file).make_preferred().string();

                frame.source = source;
                frame.line = location->line;
                frame.name += FormattedString<>(" Line %d", location->line);
            }

            response.stackFrames.push_back(frame);
        }

        return response;
    });

    // The Scopes request reports all the scopes of the given stack frame.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Scopes
    m_session->registerHandler(
        [&](const dap::ScopesRequest& request) -> dap::ResponseOrError<dap::ScopesResponse> {
            auto lock = std::lock_guard{m_frameMutex};

            dap::ScopesResponse response;

            // ScopesRequest::frameId corresponds to the StackFrame::id we set in StackTraceRequest
            int callStackIndex = request.frameId;

            auto frame = m_callStack.Frames()[callStackIndex];
            if (auto function = m_debugSymbols.GetFunctionByAddress(frame.frameAddress)) {
                dap::Scope dapScope;
                dapScope.name = "Locals";
                dapScope.presentationHint = "locals";
                dapScope.variablesReference =
                    VariableRef{VariableRef::Type::CallStackIndex, callStackIndex}.AsInt();
                response.scopes.push_back(dapScope);
            }

            return response;
        });

    // The Variables request reports all the variables for the given scope.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
    m_session->registerHandler(
        [&](const dap::VariablesRequest& request) -> dap::ResponseOrError<dap::VariablesResponse> {
            auto lock = std::lock_guard{m_frameMutex};

            dap::VariablesResponse response;

            const auto& frames = m_callStack.Frames();

            VariableRef variableRef{request.variablesReference};

            if (variableRef.GetType() == VariableRef::Type::CallStackIndex) {
                // We're about to populate top-level variables for the current call stack index, so
                // clear the dynamic variable cache.
                m_dynamicVariables.Clear();

                // VariablesRequest::variablesReference corresponds to Scope::variablesReference we
                // set in ScopesRequest
                const size_t callStackIndex = variableRef.GetValue();
                const auto frame = frames[callStackIndex];

                const bool atTop = (callStackIndex + 1) == m_callStack.Frames().size();
                const uint16_t currAddress =
                    atTop ? PC() : frames[callStackIndex + 1].calleeAddress;
                const uint16_t stackAddress =
                    atTop ? m_cpu->Registers().S : frames[callStackIndex + 1].stackPointer;

                if (auto function = m_debugSymbols.GetFunctionByAddress(frame.frameAddress)) {
                    // Collect variables from all scopes that encompass currAddress
                    Traverse(function->scope, [&](const std::shared_ptr<const Scope>& scope) {
                        // Skip scopes that do not contains the current address
                        if (!scope->Contains(currAddress))
                            return;

                        for (auto& var : scope->variables) {
                            uint16_t varAddress =
                                stackAddress + std::get<Variable::StackOffset>(var->location);

                            auto dapVar = CreateDapVariable(var, varAddress);
                            response.variables.push_back(std::move(dapVar));
                        }
                    });
                }
            } else if (variableRef.GetType() == VariableRef::Type::ParentVariableId) {
                // We are expanding a variable, get the parent variable and address
                const int id = variableRef.GetValue();
                auto [parentVar, parentVarAddress] =
                    m_dynamicVariables.GetAndRemoveVariableById(id);

                auto dapVars = CreateChildDapVariables(parentVar, parentVarAddress);
                container_move_append(response.variables, dapVars);
            }

            return response;
        });

    m_session->registerHandler([&](const dap::EvaluateRequest&) {
        // TODO: implement to support "watch" values
        dap::EvaluateResponse response;
        return response;
    });

    // The Pause request instructs the debugger to pause execution of one or all
    // threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Pause
    m_session->registerHandler([&](const dap::PauseRequest&) {
        m_requestQueue.push(DebuggerRequest::Pause);
        return dap::PauseResponse();
    });

    // The Continue request instructs the debugger to resume execution of one or
    // all threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Continue
    m_session->registerHandler([&](const dap::ContinueRequest&) {
        m_requestQueue.push(DebuggerRequest::Continue);
        return dap::ContinueResponse();
    });

    // The Next request instructs the debugger to single line step for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Next
    m_session->registerHandler([&](const dap::NextRequest&) {
        m_requestQueue.push(DebuggerRequest::StepOver);
        return dap::NextResponse();
    });

    // The StepIn request instructs the debugger to step-in for a specific thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
    m_session->registerHandler([&](const dap::StepInRequest&) {
        m_requestQueue.push(DebuggerRequest::StepInto);
        return dap::StepInResponse();
    });

    // The StepOut request instructs the debugger to step-out for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepOut
    m_session->registerHandler([&](const dap::StepOutRequest&) {
        m_requestQueue.push(DebuggerRequest::StepOut);
        return dap::StepOutResponse();
    });

    // The SetBreakpoints request instructs the debugger to clear and set a number
    // of line breakpoints for a specific source file.
    // This example debugger only exposes a single source file.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
    m_session->registerHandler([&](const dap::SetBreakpointsRequest& request) {
        (void)request;

        auto lock = std::lock_guard{m_frameMutex};

        auto addVerifiedBreakpoint = [](dap::SetBreakpointsResponse& response) {
            dap::Breakpoint bp;
            bp.verified = true;
            response.breakpoints.push_back(bp);
        };

        auto addUnverifiedBreakpoint = [](dap::SetBreakpointsResponse& response,
                                          std::string message) {
            dap::Breakpoint bp;
            bp.verified = false;
            bp.message = message;
            response.breakpoints.push_back(bp);
        };

        dap::SetBreakpointsResponse response;

        const auto breakpoints = request.breakpoints.value({});

        const auto sourcePath = request.source.path;
        if (!sourcePath) {
            for (std::size_t i = 0; i < breakpoints.size(); ++i) {
                addUnverifiedBreakpoint(response, "No source file provided");
            }
            return response;
        }

        const auto spath = MakePath(*sourcePath).generic_string();
        const auto sroot = m_sourceRoot.string();

        if (auto index = spath.find(sroot); index == std::string::npos || index != 0) {
            for (std::size_t i = 0; i < breakpoints.size(); ++i) {
                addUnverifiedBreakpoint(response, "File is not under source root");
            }
            return response;
        }

        const std::string filePath = spath.substr(sroot.size());

        // Remove all breakpoints for the current file
        m_userBreakpoints.RemoveAllIf([&](Breakpoint& bp) {
            auto location = m_debugSymbols.GetSourceLocation(bp.address);
            ASSERT(location);
            return location->file == filePath;
        });

        // Add new breakpoints for current file
        for (auto& bp : breakpoints) {
            const auto location = SourceLocation{filePath, static_cast<uint32_t>(bp.line)};
            if (auto address = m_debugSymbols.GetAddressBySourceLocation(location)) {

                m_userBreakpoints.Add(Breakpoint::Type::Instruction, *address);
                addVerifiedBreakpoint(response);
            } else {
                addUnverifiedBreakpoint(response, "Source location not found in debug info");
            }
        }

        return response;
    });

    // The SetExceptionBreakpoints request configures the debugger's handling of
    // thrown exceptions.
    // This example debugger does not use any exceptions, so this is a no-op.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetExceptionBreakpoints
    m_session->registerHandler([&](const dap::SetExceptionBreakpointsRequest&) {
        return dap::SetExceptionBreakpointsResponse();
    });

    // The Source request retrieves the source code for a given source file.
    // This example debugger only exposes one synthetic source file.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Source
    m_session->registerHandler(
        [&](const dap::SourceRequest& /*request*/) -> dap::ResponseOrError<dap::SourceResponse> {
            // if (request.sourceReference != sourceReferenceId) {
            //    return dap::Error("Unknown source reference '%d'", int(request.sourceReference));
            //}

            dap::SourceResponse response;
            // response.content = sourceContent;
            return response;
        });

    // The Launch request is made when the client instructs the debugger adapter
    // to start the debuggee. This request contains the launch arguments.
    // This example debugger does nothing with this request.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Launch
    m_session->registerHandler([&](const dap::LaunchRequest&) { return dap::LaunchResponse(); });

    // Handler for disconnect requests
    m_session->registerHandler([&](const dap::DisconnectRequest& request) {
        if (request.terminateDebuggee.value(false)) {
            // TODO: kill vectrexy if launched (support attach/detach?)
            // terminate.fire();
        }
        return dap::DisconnectResponse();
    });

    // The ConfigurationDone request is made by the client once all configuration
    // requests have been made.
    // This example debugger uses this request to 'start' the debugger.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_ConfigurationDone
    m_session->registerHandler([&](const dap::ConfigurationDoneRequest&) {
        m_configuredEvent.fire();
        return dap::ConfigurationDoneResponse();
    });
}

void DapDebugger::WaitDap() {
    // Set breakpoint on main function
    auto* mainSymbol = m_debugSymbols.GetSymbolByName("main()");
    ASSERT(mainSymbol);
    if (mainSymbol) {
        m_internalBreakpoints.Add(Breakpoint::Type::Instruction, mainSymbol->address).Once();
    }

    // All the handlers we care about have now been registered.
    // We now bind the m_session to stdin and stdout to connect to the client.
    // After the call to bind() we should start receiving requests, starting with
    // the Initialize request.
    std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
    std::shared_ptr<dap::Writer> out = dap::file(stdout, false);

    if (m_dapLog) {
        m_session->bind(spy(in, m_dapLog), spy(out, m_dapLog));
    } else {
        m_session->bind(in, out);
    }

    // Wait for the ConfigurationDone request to be made.
    m_configuredEvent.wait();

    // Broadcast the existance of the single thread to the client.
    dap::ThreadEvent threadStartedEvent;
    threadStartedEvent.reason = "started";
    threadStartedEvent.threadId = DAP_THREAD_ID;
    m_session->send(threadStartedEvent);
}

void DapDebugger::OnEvent(DapDebugger::Event event) {
    switch (event) {
    case Event::BreakpointHit: {
        // The debugger has hit a breakpoint. Inform the client.
        dap::StoppedEvent dapEvent;
        dapEvent.reason = "breakpoint";
        dapEvent.threadId = DAP_THREAD_ID;
        m_session->send(dapEvent);
    } break;

    case Event::Stepped: {
        // The debugger has single-line stepped. Inform the client.
        dap::StoppedEvent dapEvent;
        dapEvent.reason = "step";
        dapEvent.threadId = DAP_THREAD_ID;
        m_session->send(dapEvent);
    } break;

    case Event::Paused: {
        // The debugger has been suspended. Inform the client.
        dap::StoppedEvent dapEvent;
        dapEvent.reason = "pause";
        dapEvent.threadId = DAP_THREAD_ID;
        m_session->send(dapEvent);
    } break;
    }
}

void DapDebugger::ExecuteFrameInstructions(double frameTime, const Input& input,
                                           RenderContext& renderContext,
                                           AudioContext& audioContext) {

    auto lock = std::lock_guard{m_frameMutex};

    // Execute as many instructions that can fit in this time slice (plus one more at most)
    const double cpuCyclesThisFrame = Cpu::Hz * frameTime;
    m_cpuCyclesLeft += cpuCyclesThisFrame;

    auto DoExecuteInstruction = [&] {
        const cycles_t elapsedCycles = ExecuteInstruction(input, renderContext, audioContext);
        m_cpuCyclesLeft -= elapsedCycles;
    };

    // If there's a transition to make, makes it and returns true
    auto CheckForTransition = [&]() -> bool {
        if (auto request = m_requestQueue.pop()) {
            switch (*request) {
            case DebuggerRequest::Pause:
                m_state = Pausing{};
                break;
            case DebuggerRequest::Continue:
                m_state = Running{};
                break;
            case DebuggerRequest::StepOver:
                m_state = StepOver{};
                break;
            case DebuggerRequest::StepInto:
                m_state = StepInto{};
                break;
            case DebuggerRequest::StepOut:
                m_state = StepOut{};
                break;
            }
            return true;
        }
        return false;
    };

    auto BreakIntoDebugger = [&](Event event) {
        m_state = Paused{};
        OnEvent(event);
    };

    while (m_cpuCyclesLeft > 0) {
        std_util::visit_overloads(
            m_state,

            [&](Running&) {
                if (CheckForTransition())
                    return;

                DoExecuteInstruction();

                if (CheckForBreakpoints()) {
                    BreakIntoDebugger(Event::BreakpointHit);
                }
            },

            [&](Pausing&) {
                if (CheckForTransition())
                    return;

                DoExecuteInstruction();

                // Pause on the next valid source location
                const auto* postLocation = m_debugSymbols.GetSourceLocation(PC());
                if (postLocation) {
                    BreakIntoDebugger(Event::Paused);
                }
            },

            [&](StepInto& st) {
                if (CheckForTransition())
                    return;

                if (st.entry) {
                    st.entry = false;
                    ASSERT(m_debugSymbols.GetSourceLocation(PC()));
                    st.initialSourceLocation = *m_debugSymbols.GetSourceLocation(PC());
                }

                DoExecuteInstruction();
                const auto* currSourceLocation = m_debugSymbols.GetSourceLocation(PC());

                if (!currSourceLocation)
                    return;

                // Break as soon as we hit a new source location
                if (*currSourceLocation != st.initialSourceLocation) {
                    BreakIntoDebugger(Event::Stepped);
                }
            },

            [&](StepOverOrOut& st) {
                if (CheckForTransition())
                    return;

                if (st.entry) {
                    st.entry = false;
                    ASSERT(m_debugSymbols.GetSourceLocation(PC()));
                    st.initialSourceLocation = *m_debugSymbols.GetSourceLocation(PC());
                    st.initialStackDepth = m_callStack.Frames().size();
                }

                // Get source location of parent frame
                const auto calleeAddress = m_callStack.GetLastCalleeAddress();

                DoExecuteInstruction();

                if (CheckForBreakpoints()) {
                    BreakIntoDebugger(Event::BreakpointHit);
                    return;
                }

                const size_t currStackDepth = m_callStack.Frames().size();

                bool shouldBreak = false;

                // For step over or out, if our stack depth is smaller than the initial (i.e.
                // returned from a function), we break.
                if (currStackDepth < st.initialStackDepth) {

                    if (calleeAddress) {
                        const auto* currSourceLocation = m_debugSymbols.GetSourceLocation(PC());
                        const auto calleeSourceLocation =
                            m_debugSymbols.GetSourceLocation(*calleeAddress);

                        // TODO: is this right?
                        if (!currSourceLocation || !calleeSourceLocation)
                            return;

                        // If we've stepped out onto an instruction of the calling source location,
                        // then "finish" the step out. Otherwise, we're at a new SourceLocation
                        // already, so just break.
                        if (*currSourceLocation == *calleeSourceLocation) {
                            m_state = FinishStepOut{};
                            return;
                        }
                    }

                    shouldBreak = true;

                } else if (st.isStepOver) {
                    // For step over, as soon as we've changed source location, and are back at the
                    // original stack depth, we break.

                    const auto* currSourceLocation = m_debugSymbols.GetSourceLocation(PC());
                    if (!currSourceLocation)
                        return;

                    const bool sourceLocationChanged =
                        *currSourceLocation != st.initialSourceLocation;
                    const bool sameStackDepth = currStackDepth == st.initialStackDepth;

                    if (sourceLocationChanged && sameStackDepth) {
                        shouldBreak = true;
                    }
                }

                if (shouldBreak) {
                    BreakIntoDebugger(Event::Stepped);
                }
            },

            [&](FinishStepOut& st) {
                if (st.entry) {
                    st.entry = false;
                    ASSERT(m_debugSymbols.GetSourceLocation(PC()));
                    st.initialSourceLocation = *m_debugSymbols.GetSourceLocation(PC());
                }

                // Break as soon as source location changes or we are at another call instruction.
                // The latter will happen if a given SourceLocation makes multiple calls, in which
                // case, we want to stay on this same line so that the user can step into or over.

                bool shouldBreak = false;

                if (DebuggerUtil::IsCall(PC(), *m_memoryBus)) {
                    shouldBreak = true;

                } else {
                    DoExecuteInstruction();

                    if (CheckForBreakpoints()) {
                        BreakIntoDebugger(Event::BreakpointHit);
                        return;
                    }

                    const auto* currSourceLocation = m_debugSymbols.GetSourceLocation(PC());
                    if (!currSourceLocation)
                        return;

                    const bool sourceLocationChanged =
                        *currSourceLocation != st.initialSourceLocation;

                    shouldBreak = sourceLocationChanged;
                }

                if (shouldBreak) {
                    BreakIntoDebugger(Event::Stepped);
                }
            },

            [&](Paused&) {
                m_cpuCyclesLeft = 0;

                if (CheckForTransition())
                    return;
            }

        );
    }
}

cycles_t DapDebugger::ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                         AudioContext& audioContext) {
    try {

        const auto preOpRegisters = m_cpu->Registers();

        // In case exception is thrown, make sure to run certain things
        auto onExit = MakeScopedExit([&] {
            DebuggerUtil::PostOpUpdateCallstack(m_callStack, preOpRegisters, *m_cpu, *m_memoryBus);
        });

        cycles_t cpuCycles = m_emulator->ExecuteInstruction(input, renderContext, audioContext);
        return cpuCycles;
    } catch (std::exception& ex) {
        Printf("Exception caught:\n%s\n", ex.what());
    } catch (...) {
        Printf("Unknown exception caught\n");
    }

    // TODO: Send interrupt signal and break

    return static_cast<cycles_t>(0);
};

bool DapDebugger::CheckForBreakpoints() {
    auto check = [&](Breakpoints& breakpoints) {
        if (auto bp = breakpoints.Get(PC())) {
            if (bp->type == Breakpoint::Type::Instruction) {
                if (bp->once) {
                    breakpoints.Remove(PC());
                    return true;
                } else if (bp->enabled) {
                    Printf("Breakpoint hit at %04x\n", bp->address);
                    return true;
                }
            }
        }
        return false;
    };

    if (check(m_internalBreakpoints)) {
        return true;
    }
    if (check(m_userBreakpoints)) {
        return true;
    }

    // Handle conditional breakpoints
    {
        bool shouldBreak = false;
        auto& conditionals = m_internalConditionalBreakpoints.Breakpoints();
        for (auto iter = conditionals.begin(); iter != conditionals.end();) {
            auto& bp = *iter;
            if (bp.conditionFunc()) {
                if (bp.once) {
                    iter = conditionals.erase(iter);
                } else {
                    ++iter;

                    // TODO: output something useful, like the condition text.
                    Printf("Conditional breakpoint hit.\n");
                }
                shouldBreak = true;
            } else {
                ++iter;
            }
        }

        if (shouldBreak)
            return true;
    }

    return false;
}

uint16_t DapDebugger::PC() const {
    return m_cpu->Registers().PC;
}

dap::Variable DapDebugger::CreateDapVariable(std::shared_ptr<Variable> var, uint16_t varAddress) {

    auto MakeDapVariable = [](const std::string varName, const std::string& displayValue,
                              const std::string& displayType, int variablesReference) {
        dap::Variable dapVar;
        dapVar.name = varName;
        dapVar.value = displayValue;
        dapVar.type = displayType;
        dapVar.variablesReference = variablesReference;
        return dapVar;
    };

    auto ReadValue = [&](auto& value, size_t byteSize, uint16_t address) {
        // buffer to store value, at most 8 bytes
        // char value[8];
        ASSERT(byteSize <= std::size(value));
        // TODO: endian-dependent
        for (size_t i = byteSize; i-- > 0;) {
            value[i] = m_memoryBus->Read(address++);
        }
    };

    if (auto primType = std::dynamic_pointer_cast<PrimitiveType>(var->type)) {
        std::string displayValue;

        char value[8];
        ReadValue(value, primType->byteSize, varAddress);

        switch (primType->format) {
        case PrimitiveType::Format::Int: {
            switch (primType->byteSize) {
            case 1: {
                uint8_t v;
                std::memcpy(&v, value, sizeof(v));
                displayValue =
                    primType->isSigned ? std::to_string(static_cast<int8_t>(v)) : std::to_string(v);
            } break;
            case 2: {
                uint16_t v;
                std::memcpy(&v, value, sizeof(v));
                displayValue = primType->isSigned ? std::to_string(static_cast<int16_t>(v))
                                                  : std::to_string(v);
            } break;
            case 4: {
                uint32_t v;
                std::memcpy(&v, value, sizeof(v));
                displayValue = primType->isSigned ? std::to_string(static_cast<int32_t>(v))
                                                  : std::to_string(v);
            } break;
            case 8: {
                uint64_t v;
                std::memcpy(&v, value, sizeof(v));
                displayValue = primType->isSigned ? std::to_string(static_cast<int64_t>(v))
                                                  : std::to_string(v);
            } break;
            default:
                FAIL_MSG("Unexpected byte size");
                break;
            }
        } break;

        case PrimitiveType::Format::Char: {
            ASSERT(primType->byteSize == 1);
            uint8_t v;
            std::memcpy(&v, value, sizeof(v));
            displayValue =
                primType->isSigned ? std::to_string(static_cast<int8_t>(v)) : std::to_string(v);
        } break;

        case PrimitiveType::Format::Float: {
            switch (primType->byteSize) {
            case 4: {
                float v;
                static_assert(sizeof(v) == 4);
                std::memcpy(&v, value, sizeof(v));
                displayValue = std::to_string(v);
            } break;
            case 8: {
                double v;
                static_assert(sizeof(v) == 8);
                std::memcpy(&v, value, sizeof(v));
                displayValue = std::to_string(v);
            }
            default:
                FAIL_MSG("Unexpected byte size");
                break;
            }
        } break;
        } // switch

        return MakeDapVariable(var->name, displayValue, primType->name, 0);

    } else if (auto enumType = std::dynamic_pointer_cast<EnumType>(var->type)) {
        char value[8];
        ReadValue(value, enumType->byteSize, varAddress);

        int64_t enumValue{};

        switch (enumType->byteSize) {
        case 1: {
            uint8_t v;
            std::memcpy(&v, value, sizeof(v));
            enumValue = enumType->isSigned ? static_cast<int8_t>(v) : v;
        } break;
        case 2: {
            uint16_t v;
            std::memcpy(&v, value, sizeof(v));
            enumValue = enumType->isSigned ? static_cast<int16_t>(v) : v;
        } break;
        case 4: {
            uint32_t v;
            std::memcpy(&v, value, sizeof(v));
            enumValue = enumType->isSigned ? static_cast<int32_t>(v) : v;
        } break;
        case 8: {
            uint64_t v;
            std::memcpy(&v, value, sizeof(v));
            enumValue = enumType->isSigned ? static_cast<int64_t>(v) : v;
        } break;
        default:
            FAIL_MSG("Unexpected byte size");
            break;
        }

        std::string displayValue;
        auto iter = enumType->valueToId.find(enumValue);
        if (iter != enumType->valueToId.end()) {
            displayValue = iter->second;
        } else {
            displayValue = "<Unknown>";
        }
        displayValue += " {" + std::to_string(enumValue) + "}";

        return MakeDapVariable(var->name, displayValue, enumType->name, 0);

    } else if (auto structType = std::dynamic_pointer_cast<StructType>(var->type)) {

        std::string displayValue = ""; // TODO: display first few members?
        const int id = m_dynamicVariables.AddVariable(var, varAddress);
        int variablesReference = VariableRef{VariableRef::Type::ParentVariableId, id}.AsInt();
        return MakeDapVariable(var->name, displayValue, structType->name, variablesReference);

    } else if (auto indirectType = std::dynamic_pointer_cast<IndirectType>(var->type)) {

        const uint16_t pointeeAddress = m_memoryBus->Read16(varAddress);
        std::string displayValue = FormattedString("0x%04x", pointeeAddress).Value();

        const int id = m_dynamicVariables.AddVariable(var, varAddress);
        int variablesReference = VariableRef{VariableRef::Type::ParentVariableId, id}.AsInt();

        return MakeDapVariable(var->name, displayValue, indirectType->name, variablesReference);

    } else if (auto arrayType = std::dynamic_pointer_cast<ArrayType>(var->type)) {

        // const uint16_t arrayStartAddress = m_memoryBus->Read16(varAddress);
        std::string displayValue = FormattedString("0x%04x", varAddress).Value();

        const int id = m_dynamicVariables.AddVariable(var, varAddress);
        int variablesReference = VariableRef{VariableRef::Type::ParentVariableId, id}.AsInt();

        return MakeDapVariable(var->name, displayValue, arrayType->name, variablesReference);

    } else {
        FAIL_MSG("Unexpected type");
    }

    FAIL_MSG("Unreachable");
    return {};
}

std::vector<dap::Variable> DapDebugger::CreateChildDapVariables(std::shared_ptr<Variable> parentVar,
                                                                uint16_t parentVarAddress) {
    std::vector<dap::Variable> result;

    if (auto structType = std::dynamic_pointer_cast<StructType>(parentVar->type)) {

        for (auto& m : structType->members) {
            // Create a child variable to display the dereferenced value
            auto var = std::make_shared<Variable>();
            var->name = m.name;
            var->type = m.type;

            // TODO: add support for bitfields
            const uint16_t memberAddress =
                parentVarAddress + static_cast<uint16_t>(m.offsetBits / 8);

            result.push_back(CreateDapVariable(var, memberAddress));
        }

    } else if (auto indirectType = std::dynamic_pointer_cast<IndirectType>(parentVar->type)) {

        const uint16_t pointeeAddress = m_memoryBus->Read16(parentVarAddress);
        auto var = std::make_shared<Variable>();
        var->name = "*" + parentVar->name;
        var->type = indirectType->type;
        result.push_back(CreateDapVariable(var, pointeeAddress));

    } else if (auto arrayType = std::dynamic_pointer_cast<ArrayType>(parentVar->type)) {
        const size_t elemSize = arrayType->type->Size();

        for (size_t i = 0; i < arrayType->numElems; ++i) {
            auto var = std::make_shared<Variable>();
            var->name = parentVar->name + "[" + std::to_string(i) + "]";
            var->type = arrayType->type;

            const uint16_t elemAddress =
                checked_static_cast<uint16_t>(parentVarAddress + elemSize * i);

            result.push_back(CreateDapVariable(var, elemAddress));
        }

    } else {
        FAIL_MSG("Unexpected type");
    }

    return result;
}
