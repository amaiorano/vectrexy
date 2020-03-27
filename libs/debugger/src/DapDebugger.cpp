#include "debugger/DapDebugger.h"
#include "debugger/DebuggerUtil.h"
#include "debugger/RstParser.h"
#include "emulator/Cpu.h"
#include "emulator/Emulator.h"

#include "dap/io.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include <condition_variable>
#include <memory>
#include <mutex>

namespace {
    const dap::integer threadId = 100;

    // Event provides a basic wait and signal synchronization primitive.
    class Event {
    public:
        // wait() blocks until the event is fired.
        void wait() {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&] { return fired; });
        }

        // fire() sets signals the event, and unblocks any calls to wait().
        void fire() {
            std::unique_lock<std::mutex> lock(mutex);
            fired = true;
            cv.notify_all();
        }

    private:
        std::mutex mutex;
        std::condition_variable cv;
        bool fired = false;
    };

    // Move to member
    Event g_configured;
    std::shared_ptr<dap::Writer> g_log = dap::file("dap_log.txt");

} // namespace

DapDebugger::DapDebugger() = default;
DapDebugger::~DapDebugger() = default;

void DapDebugger::Init(Emulator& emulator) {
    m_emulator = &emulator;
    m_cpu = &m_emulator->GetCpu();
    m_memoryBus = &emulator.GetMemoryBus();
    InitDap();
}

void DapDebugger::Reset() {
    m_callStack.Clear();
}

void DapDebugger::OnRomLoaded(const char* file) {
    m_sourceRoot = fs::path{file}.remove_filename().generic_string();

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

    if (!m_paused) {
        ExecuteFrameInstructions(frameTime, input, renderContext, audioContext);
    }

    return true;
}

void DapDebugger::ParseRst(const fs::path& rstFile) {
    RstParser parser{m_debugSymbols};
    parser.Parse(rstFile);
}

void DapDebugger::InitDap() {
    m_session = dap::Session::create();

    // Handle errors reported by the Session. These errors include protocol
    // parsing errors and receiving messages with no handler.
    m_session->onError([&](const char* msg) {
        Printf("dap::Session error: %s\n", msg);
        if (g_log) {
            dap::writef(g_log, "dap::Session error: %s\n", msg);
            g_log->close();
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
    // This example debugger only exposes a single thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Threads
    m_session->registerHandler([&](const dap::ThreadsRequest&) {
        dap::ThreadsResponse response;
        dap::Thread thread;
        thread.id = threadId;
        thread.name = "MainThread";
        response.threads.push_back(thread);
        return response;
    });

    // The StackTrace request reports the stack frames (call stack) for a given
    // thread. This example debugger only exposes a single stack frame for the
    // single thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StackTrace
    m_session->registerHandler([&](const dap::StackTraceRequest& request)
                                   -> dap::ResponseOrError<dap::StackTraceResponse> {
        if (request.threadId != threadId) {
            return dap::Error("Unknown threadId '%d'", int(request.threadId));
        }

        dap::StackTraceResponse response;

        size_t i = 0;
        const auto& frames = m_callStack.Frames();
        for (auto iter = frames.rbegin(); iter != frames.rend(); ++iter, ++i) {

            dap::StackFrame frame;
            frame.column = 1;
            frame.id = iter->frameAddress;

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
    // This example debugger only exposes a single 'Locals' scope for the single
    // frame.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Scopes
    m_session->registerHandler(
        [&](const dap::ScopesRequest & /*request*/) -> dap::ResponseOrError<dap::ScopesResponse> {
            // if (request.frameId != frameId) {
            //   return dap::Error("Unknown frameId '%d'", int(request.frameId));
            // }

            // dap::Scope scope;
            // scope.name = "Locals";
            // scope.presentationHint = "locals";
            // scope.variablesReference = variablesReferenceId;

            dap::ScopesResponse response;
            // response.scopes.push_back(scope);
            return response;
        });

    // The Variables request reports all the variables for the given scope.
    // This example debugger only exposes a single 'currentLine' variable for the
    // single 'Locals' scope.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Variables
    // m_session->registerHandler([&](const dap::VariablesRequest& request)
    //                              -> dap::ResponseOrError<dap::VariablesResponse> {
    //   if (request.variablesReference != variablesReferenceId) {
    //     return dap::Error("Unknown variablesReference '%d'",
    //                       int(request.variablesReference));
    //   }

    //   dap::Variable currentLineVar;
    //   currentLineVar.name = "currentLine";
    //   currentLineVar.value = std::to_string(debugger.currentLine());
    //   currentLineVar.type = "int";

    //   dap::VariablesResponse response;
    //   response.variables.push_back(currentLineVar);
    //   return response;
    // });

    // The Pause request instructs the debugger to pause execution of one or all
    // threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Pause
    m_session->registerHandler([&](const dap::PauseRequest&) {
        // TODO
        // debugger.pause();
        assert(!m_paused);
        m_runState = RunState::Pausing;
        return dap::PauseResponse();
    });

    // The Continue request instructs the debugger to resume execution of one or
    // all threads.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Continue
    m_session->registerHandler([&](const dap::ContinueRequest&) {
        // TODO
        // debugger.run();
        assert(m_paused);
        m_runState = RunState::Running;
        m_paused = false;
        return dap::ContinueResponse();
    });

    // The Next request instructs the debugger to single line step for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_Next
    m_session->registerHandler([&](const dap::NextRequest&) {
        // TODO
        // debugger.stepForward();
        return dap::NextResponse();
    });

    // The StepIn request instructs the debugger to step-in for a specific thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepIn
    m_session->registerHandler([&](const dap::StepInRequest&) {
        // TODO
        // debugger.stepForward();
        assert(m_paused);
        m_runState = RunState::StepInto;
        m_paused = false;
        return dap::StepInResponse();
    });

    // The StepOut request instructs the debugger to step-out for a specific
    // thread.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_StepOut
    m_session->registerHandler([&](const dap::StepOutRequest&) {
        // TODO
        return dap::StepOutResponse();
    });

    // The SetBreakpoints request instructs the debugger to clear and set a number
    // of line breakpoints for a specific source file.
    // This example debugger only exposes a single source file.
    // https://microsoft.github.io/debug-adapter-protocol/specification#Requests_SetBreakpoints
    m_session->registerHandler([&](const dap::SetBreakpointsRequest& request) {
        (void)request;

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

        // Start by clearing all breakpoints. The protocol is that every time a breakpoint is added
        // or removed, all enabled breakpoints will be sent in this request.
        m_userBreakpoints.RemoveAll();

        const auto breakpoints = request.breakpoints.value({});
        if (breakpoints.empty()) {
            return response;
        }

        const auto sourcePath = request.source.path;
        if (!sourcePath) {
            for (std::size_t i = 0; i < breakpoints.size(); ++i) {
                addUnverifiedBreakpoint(response, "No source file provided");
            }
            return response;
        }

        const auto spath = fs::path{*sourcePath}.generic_string();
        const auto sroot = m_sourceRoot.string();

        if (auto index = spath.find(sroot); index == std::string::npos || index != 0) {
            for (std::size_t i = 0; i < breakpoints.size(); ++i) {
                addUnverifiedBreakpoint(response, "File is not under source root");
            }
            return response;
        }

        const std::string filePath = spath.substr(sroot.size());

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
        [&](const dap::SourceRequest & /*request*/) -> dap::ResponseOrError<dap::SourceResponse> {
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
        g_configured.fire();
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

    if (g_log) {
        m_session->bind(spy(in, g_log), spy(out, g_log));
    } else {
        m_session->bind(in, out);
    }

    // Wait for the ConfigurationDone request to be made.
    g_configured.wait();

    // Broadcast the existance of the single thread to the client.
    dap::ThreadEvent threadStartedEvent;
    threadStartedEvent.reason = "started";
    threadStartedEvent.threadId = threadId;
    m_session->send(threadStartedEvent);
}

void DapDebugger::OnEvent(DapDebugger::Event event) {
    switch (event) {
    case Event::BreakpointHit: {
        // The debugger has hit a breakpoint. Inform the client.
        dap::StoppedEvent dapEvent;
        dapEvent.reason = "breakpoint";
        dapEvent.threadId = threadId;
        m_session->send(dapEvent);
    } break;

    case Event::Stepped: {
        // The debugger has single-line stepped. Inform the client.
        dap::StoppedEvent dapEvent;
        dapEvent.reason = "step";
        dapEvent.threadId = threadId;
        m_session->send(dapEvent);
    } break;

    case Event::Paused: {
        // The debugger has been suspended. Inform the client.
        dap::StoppedEvent dapEvent;
        dapEvent.reason = "pause";
        dapEvent.threadId = threadId;
        m_session->send(dapEvent);
    } break;
    }
}

void DapDebugger::ExecuteFrameInstructions(double frameTime, const Input& input,
                                           RenderContext& renderContext,
                                           AudioContext& audioContext) {
    // Execute as many instructions that can fit in this time slice (plus one more at most)
    const double cpuCyclesThisFrame = Cpu::Hz * frameTime;
    m_cpuCyclesLeft += cpuCyclesThisFrame;

    auto BreakIntoDebugger = [&] {
        m_paused = true;
        m_cpuCyclesLeft = 0;
    };

    while (m_cpuCyclesLeft > 0) {
        const auto* preLocation = m_debugSymbols.GetSourceLocation(PC());

        const cycles_t elapsedCycles = ExecuteInstruction(input, renderContext, audioContext);

        // NOTE: We check for breakpoints after executing an instruction so that stepping while on
        // an instruction breakpoint doesn't keep hitting that breakpoint. But this means there's no
        // way to set a breakpoint on the very first instruction that will get executed. Fix this by
        // remembering the last PC, and only processing breakpoints if the current != last PC.
        if (CheckForBreakpoints()) {
            BreakIntoDebugger();
            OnEvent(Event::BreakpointHit);
            return;
        }

        switch (m_runState) {
        case RunState::Running:
            break;

        case RunState::Pausing: {
            // Pause on the next valid source location
            const auto* postLocation = m_debugSymbols.GetSourceLocation(PC());
            if (postLocation) {
                BreakIntoDebugger();
                OnEvent(Event::Paused);
                return;
            }
        } break;

        case RunState::StepInto: {
            const auto* postLocation = m_debugSymbols.GetSourceLocation(PC());
            if ((preLocation && postLocation) && (*preLocation != *postLocation)) {
                BreakIntoDebugger();
                OnEvent(Event::Stepped);
                return;
            }
        } break;
        }

        m_cpuCyclesLeft -= elapsedCycles;
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
    // BreakIntoDebugger();

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
    return false;
}

uint16_t DapDebugger::PC() const {
    return m_cpu->Registers().PC;
}
