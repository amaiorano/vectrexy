#include "debugger/GdbStub.h"
#include "core/ConsoleOutput.h"
#include "core/StringUtil.h"
#include "emulator/Cpu.h"
#include "emulator/Emulator.h"
#include <cassert>
#include <thread>

// GDB commands:
// Enable debugging of remote protocol (shows commands send/received from GDB):
//      set debug remote 1
//
// How to connect to remote on localhost (default) on specific port:
//      target remote :65520

// TODO: Move to Base.h
// overload() returns an object derived from multiple callables, producing a single object with
// overloads of the call functions. Can be used to std::visit lambdas over a variant.
template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};
template <typename... Ts>
overload(Ts...)->overload<Ts...>;

// template <typename... Ts, typename... Us>
// auto visit_overloads(std::variant<Ts...>& variant, overload<Us...>& overload) {
//    return std::visit(overload, variant);
//}

#define TODO(msg) assert(!msg)

namespace {
    constexpr uint16_t GdbPort = 65520;

    template <typename... Args>
    void GdbPrintf(const char* format, Args... args) {
        Printf("[GdbStub] ");
        Printf(format, std::forward<Args>(args)...);
        FlushStream(ConsoleStream::Output);
    }

    namespace Signal {
        constexpr int Zero = 0;      // Running?
        constexpr int Interrupt = 2; // Break/pause
        constexpr int Illegal = 4;   // Illegal instruction (useful?)
        constexpr int Trap = 5;      // Trap/breakpoint

    } // namespace Signal

} // namespace

void GdbStub::Init(Emulator& emulator) {
    m_emulator = &emulator;
}

void GdbStub::Reset() {
    TODO("Implement GdbStub::Reset");
}

bool GdbStub::Connected() const {
    return m_socket.Connected();
}

void GdbStub::Connect() {
    m_socket.Open(GdbPort);
    Errorf("Waiting for connection...\n");
    while (!m_socket.TryAccept()) {
        // Errorf("Server: no connection, retrying...\n");
        Errorf(".");
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }
    Errorf("Connected!\n");

    // We start in stopped state. GDB will send us a '?' to query the current state, and we will
    // reply with m_lastSignal.
    m_lastSignal = Signal::Interrupt;
    m_running = false;
}

namespace {
    enum class GdbResult {
        Ok,
        BadChecksum,
        Break,
        ReadError,
        WriteError,
    };

    int8_t HexCharToInt(char c) {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return 10 + c - 'a';
        if (c >= 'A' && c <= 'F')
            return 10 + c - 'A';
        return -1;
    }

    int8_t HexToInt8(const char* s) {
        int8_t msb = HexCharToInt(s[0]);
        int8_t lsb = HexCharToInt(s[1]);
        if (msb < 0 || lsb < 0)
            return -1;
        return (msb << 4) | lsb;
    }

    uint8_t HexToUInt8(const char* s) { return static_cast<uint8_t>(HexToInt8(s)); }

    int16_t HexToInt16(const char* s) {
        int8_t msb = HexToInt8(s);
        int8_t lsb = HexToInt8(s + 2);
        if (msb < 0 || lsb < 0)
            return -1;
        return (msb << 8) | lsb;
    }

    uint16_t HexToUInt16(const char* s) { return static_cast<uint16_t>(HexToInt16(s)); }

    template <typename T>
    T HexStringToInt(const std::string& s) {
        return static_cast<T>(std::stoi(s, nullptr, 16));
    }

    GdbResult ReadPacket(TcpServer& socket, std::vector<uint8_t>& data) {
        // Gdb sends "$<packet data>#<2 byte checksum>"

        data.clear();

        // States
        struct Waiting {};
        struct ReadingData {
            uint8_t localChecksum = 0;
        };
        struct ReadingChecksum {
            uint8_t localChecksum = 0;
            uint8_t remoteChecksum = 0;
            bool firstByte = true;
        };
        struct Done {
            GdbResult result = GdbResult::Ok;
        };

        std::variant<Waiting, ReadingData, ReadingChecksum, Done> state = {Waiting{}};
        uint8_t byte;

        while (socket.Receive(byte)) {
            auto sm = overload //
                {[&](Waiting&) {
                     if (byte == '$')
                         state = ReadingData{};
                     else if (byte == 3)
                         state = Done{GdbResult::Break};
                 },
                 [&](ReadingData& s) {
                     if (byte == '#') {
                         state = ReadingChecksum{s.localChecksum};
                         return;
                     }

                     data.push_back(byte);
                     s.localChecksum += byte;
                 },
                 [&](ReadingChecksum& s) {
                     int v = HexCharToInt(byte);
                     if (v < 0) {
                         // TODO: error?
                         state = Waiting{};
                         return;
                     }

                     // Checksum comes in as 2 hex chars, so combine them into a decimal byte and
                     // compare to our own checksum
                     if (s.firstByte) {
                         s.firstByte = false;
                         s.remoteChecksum = (static_cast<uint8_t>(v) << 4);
                     } else {
                         s.remoteChecksum |= static_cast<uint8_t>(v);

                         if (s.remoteChecksum != s.localChecksum) {
                             state = Done{GdbResult::BadChecksum};
                             return;
                         }
                         state = Done{};
                     }
                 },
                 [&](Done&) {}};
            std::visit(sm, state);

            if (auto s = std::get_if<Done>(&state)) {
                // We interpret data as a string, so add a null terminator
                data.push_back('\0');
                return s->result;
            }
        }

        return GdbResult::Ok;
    }

    uint8_t checksum(std::string_view s) {
        uint8_t cs = 0;
        for (auto c : s) {
            cs += c;
        }
        return cs;
    }

    // Sends a GDB packet: $packet-data#checksum
    void SendPacket(TcpServer& socket, std::string_view s) {
        // All GDB commands and responses (other than acknowledgments and notifications, see
        // Notification Packets) are sent as a packet. A packet is introduced with the character
        // ‘$’, the actual packet-data, and the terminating character ‘#’ followed by a two-digit
        // checksum: $packet - data#checksum The two - digit checksum is computed as the modulo 256
        // sum of all characters between the leading ‘$’ and the trailing ‘#’(an eight bit unsigned
        // checksum).
        std::string packet = FormattedString<1025>("$%s#%02x", s.data(), checksum(s));
        GdbPrintf("  Send ->: %s\n", packet.c_str());
        socket.Send(packet.data(), static_cast<int>(packet.size()));
    }

    // Sends string as buffer, not including the null terminator
    void SendString(TcpServer& socket, std::string_view s) {
        GdbPrintf("  Send ->: %s\n", s.data());
        socket.Send(s.data(), static_cast<int>(s.length()));
    }

    void SendSignal(TcpServer& socket, int signal) {
        // Signal format: S AA
        // Program received signal AA (two-digit hex number)
        SendPacket(socket, FormattedString<4>("S%02x", signal).Value());
    }

    void SendRegisters(TcpServer& socket, Cpu& cpu) {
        // g
        // Read general registers.

        auto& r = cpu.Registers();
        auto packet = FormattedString<>("%02x%02x%02x%02x%04x%04x%04x%04x%04x"
                                        "xxxxxxxxxx", // 5x 6309 reg values
                                        r.CC, r.A, r.B, r.DP, r.X, r.Y, r.U, r.S, r.PC);
        SendPacket(socket, packet.Value());
    }

    void SetRegisters(TcpServer& socket, Cpu& cpu, std::string_view args) {
        // G XX…
        // Write general registers.

        // We expect 38 values
        if (args.length() != 38) {
            SendString(socket, "E00");
            return;
        }

        // Copy registers
        auto r = cpu.Registers();

        if (auto v = HexToInt8(args.data() + 0); v >= 0) {
            r.CC.Value = v;
        }
        if (auto v = HexToInt8(args.data() + 2); v >= 0) {
            r.A = v;
        }
        if (auto v = HexToInt8(args.data() + 4); v >= 0) {
            r.B = v;
        }
        if (auto v = HexToInt8(args.data() + 6); v >= 0) {
            r.DP = v;
        }
        if (auto v = HexToInt16(args.data() + 8); v >= 0) {
            r.X = v;
        }
        if (auto v = HexToInt16(args.data() + 12); v >= 0) {
            r.Y = v;
        }
        if (auto v = HexToInt16(args.data() + 16); v >= 0) {
            r.U = v;
        }
        if (auto v = HexToInt16(args.data() + 20); v >= 0) {
            r.S = v;
        }
        if (auto v = HexToInt16(args.data() + 24); v >= 0) {
            r.PC = v;
        }

        cpu.SetRegisters(r);

        SendPacket(socket, "OK");
    }

    void SendRegister(TcpServer& socket, Cpu& cpu, std::string_view args) {
        // p n
        // Read the value of register n; n is in hex.

        const auto& reg = cpu.Registers();

        auto regIndex = HexStringToInt<int>(std::string{args});
        auto [value, size] = [&]() -> std::tuple<int, int> {
            switch (regIndex) {
            case 0:
                return {reg.CC.Value, 1};
            case 1:
                return {reg.A, 1};
            case 2:
                return {reg.B, 1};
            case 3:
                return {reg.DP, 1};
            case 4:
                return {reg.X, 2};
            case 5:
                return {reg.Y, 2};
            case 6:
                return {reg.U, 2};
            case 7:
                return {reg.S, 2};
            case 8:
                return {reg.PC, 2};
            default:
                return {0, 0};
            }
        }();

        switch (size) {
        case 1:
            SendPacket(socket, FormattedString<3>("%02x", value).Value());
            break;
        case 2:
            SendPacket(socket, FormattedString<5>("%04x", value).Value());
            break;
        default:
            SendPacket(socket, "E00");
        };
    }

    void SetRegister(TcpServer& socket, Cpu& cpu, std::string_view args) {
        // P n…=r…
        // Write register n… with value r…. The register number n is in hexadecimal, and r… contains
        // two hex digits for each byte in the register (target byte order).

        const auto sargs = StringUtil::Split(std::string{args}, "=");
        auto regIndex = HexStringToInt<int>(sargs[0]);
        auto value = HexStringToInt<uint8_t>(sargs[1]);

        auto reg = cpu.Registers();

        switch (regIndex) {
        case 0:
            reg.CC.Value = value;
            break;
        case 1:
            reg.A = value;
            break;
        case 2:
            reg.B = value;
            break;
        case 3:
            reg.DP = value;
            break;
        case 4:
            reg.X = value;
            break;
        case 5:
            reg.Y = value;
            break;
        case 6:
            reg.U = value;
            break;
        case 7:
            reg.S = value;
            break;
        case 8:
            reg.PC = value;
            break;
        default:
            SendPacket(socket, "E00");
        };

        SendPacket(socket, "OK");
    }

    void SendMemory(TcpServer& socket, MemoryBus& memoryBus, std::string_view args) {
        // m addr,length
        // Read length addressable memory units starting at address addr.

        auto sargs = StringUtil::Split(std::string{args}, ",");
        auto addr = HexStringToInt<uint16_t>(sargs[0]);
        auto length = HexStringToInt<uint16_t>(sargs[1]);

        // Send each byte one at a time, and compute checksum to send at the end
        uint8_t checksum = 0;
        for (uint16_t i = 0; i < length; ++i) {
            uint8_t byte = memoryBus.ReadRaw(addr + i);
            auto packet = FormattedString<3>("%02x", byte);

            checksum += packet.Value()[0];
            checksum += packet.Value()[1];

            SendPacket(socket, packet.Value());
        }

        // Send checksum
        auto packet = FormattedString<4>("#%02x", checksum);
        SendPacket(socket, packet.Value());
    }

    void SetMemory(TcpServer& socket, MemoryBus& memoryBus, std::string_view args) {
        // M addr,length:XX…
        // Write length addressable memory units starting at address addr. The data is given by XX…;
        // each byte is transmitted as a two-digit hexadecimal number.

        auto sargs2 = StringUtil::Split(std::string{args}, ":");
        auto sargs1 = StringUtil::Split(sargs2[0], ",");
        auto addr = HexStringToInt<uint16_t>(sargs1[0]);
        auto length = HexStringToInt<uint16_t>(sargs1[1]);

        // XX...
        const char* values = sargs2[1].data();

        for (uint16_t i = 0; i < length; ++i) {
            uint8_t byte = HexToUInt8(values);
            values += 2;
            memoryBus.Write(addr, byte);
        }

        SendPacket(socket, "OK");
    }

    void GeneralQuery(TcpServer& socket, std::string_view args) {
        auto sargs = StringUtil::Split(std::string{args}, ":");

        if (sargs[0] == "Supported") {
            // args contains the list of features that GDB supports.
            // e.g.: "qSupported:multiprocess+;qRelocInsn+n"
            // We don't really care about that. We reply with the features
            // that we support.
            SendPacket(socket, "PacketSize=1024"
                       //";ConditionalBreakpoints-"
                       //";ConditionalTracepoints-"
                       //";EnableDisableTracepoints-"
                       //";BreakpointCommands-"
            );
        } else if (sargs[0] == "Attached") {
            // Once attached, we're always attached.
            // TODO: We may want to support detaching when switching game, resetting, or if user
            // selects an option to detach.
            SendPacket(socket, "1");
        } else {
            // Unknown query, send dummy response, which GDB interprets as not supported.
            SendPacket(socket, "");
        }
    }

    void GeneralSet(TcpServer& socket, std::string_view /*args*/) {
        // Not supporting anything yet, send dummy response
        SendPacket(socket, "");
    }

} // namespace

bool GdbStub::FrameUpdate(double frameTime, const EmuEvents& emuEvents, const Input& input,
                          RenderContext& renderContext, AudioContext& audioContext) {
    (void)frameTime;
    (void)emuEvents;
    (void)input;
    (void)renderContext;
    (void)audioContext;

    constexpr char* ACK = "+"; // Packet received correctly
    constexpr char* NAK = "-"; // Request retransmission

    if (m_socket.ReceiveDataAvailable()) {
        std::vector<uint8_t> data;
        auto result = ReadPacket(m_socket, data);

        // TODO: Do we really want to return here? Maybe stick this socket stuff in a function, so
        // we can return here and continue emulator execution.
        if (data.empty())
            return true;

        GdbPrintf("Recv <-: %s\n", data.data());

        if (result == GdbResult::Break) {
            // TODO: pause emulator
        } else if (result == GdbResult::BadChecksum) {
            SendString(m_socket, NAK);
            return false;
        } else if (result != GdbResult::Ok) {
            // TODO: send NAK?
            return false;
        }

        // ACK read packet
        SendString(m_socket, ACK);

        // Process the packet command
        const char command = data[0];
        // First char is command, rest is args
        auto args = std::string_view{reinterpret_cast<const char*>(data.data() + 1)};

        switch (command) {
        case '?': // Indicate reason target halted
            SendSignal(m_socket, m_lastSignal);
            break;

        case 'c': // Continue
            // Note args may contain an address to continue at, which we ignore right now. TODO:
            // check for arg and send NACK?

            // For continue, we need to send an interrupt signal once we break again (for
            // breakpoint, or user presses Ctrl+C, etc.)

            // TODO("handle 'c' for continue");
            m_lastSignal = Signal::Zero;
            // SendSignal(m_socket, m_lastSignal);
            m_running = true;

            // Reset
            m_cpuCyclesLeft = 0;

            break;

        case 'D': // Detach
            // Kill socket connection?
            TODO("handle 'D' for detach");
            break;

        case 'g': // Send registers to GDB
            SendRegisters(m_socket, m_emulator->GetCpu());
            break;

        case 'G': // Set registers from GDB
            SetRegisters(m_socket, m_emulator->GetCpu(), args);
            break;

        case 'm': // Send memory
            SendMemory(m_socket, m_emulator->GetMemoryBus(), args);
            break;

        case 'M':
            SetMemory(m_socket, m_emulator->GetMemoryBus(), args);
            break;

        case 'p':
            SendRegister(m_socket, m_emulator->GetCpu(), args);
            break;

        case 'P':
            SetRegister(m_socket, m_emulator->GetCpu(), args);
            break;

        case 'q':
            GeneralQuery(m_socket, args);
            break;

        case 'Q':
            GeneralSet(m_socket, args);
            break;

        case 's':
            TODO("Handle 's' for single step");
            break;

        case 'z':
            TODO("Handle 'z' for remove breakpoint");
            break;

        case 'Z':
            TODO("Handle 'Z' for add breakpoint");
            break;

        default:
            // For any command not supported by the stub, an empty response (‘$#00’) should be
            // returned.
            SendString(m_socket, "$#00");
            break;
        }
    }

    if (m_running) {
        ExecuteFrameInstructions(frameTime, input, renderContext, audioContext);
    }

    return true;
}

void GdbStub::ExecuteFrameInstructions(double frameTime, const Input& input,
                                       RenderContext& renderContext, AudioContext& audioContext) {
    // Execute as many instructions that can fit in this time slice (plus one more at most)
    const double cpuCyclesThisFrame = Cpu::Hz * frameTime;
    m_cpuCyclesLeft += cpuCyclesThisFrame;

    while (m_cpuCyclesLeft > 0) {
        // if (auto bp = m_breakpoints.Get(m_cpu->Registers().PC)) {
        //    if (bp->type == Breakpoint::Type::Instruction) {
        //        if (bp->autoDelete) {
        //            m_breakpoints.Remove(m_cpu->Registers().PC);
        //            BreakIntoDebugger();
        //        } else if (bp->enabled) {
        //            Printf("Breakpoint hit at %04x\n", bp->address);
        //            BreakIntoDebugger();
        //        }
        //    }
        //}

        // if (m_breakIntoDebugger) {
        //    m_cpuCyclesLeft = 0;
        //    break;
        //}

        const cycles_t elapsedCycles = ExecuteInstruction(input, renderContext, audioContext);

        //m_cpuCyclesTotal += elapsedCycles;
        m_cpuCyclesLeft -= elapsedCycles;

        //if (m_numInstructionsToExecute && (--m_numInstructionsToExecute.value() == 0)) {
        //    m_numInstructionsToExecute = {};
        //    BreakIntoDebugger();
        //}

        // TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
        //if (m_breakIntoDebugger) {
        //    m_cpuCyclesLeft = 0;
        //    break;
        //}
    }
}

cycles_t GdbStub::ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                     AudioContext& audioContext) {
    try {
        //Trace::InstructionTraceInfo traceInfo;
        //if (m_traceEnabled) {
        //    m_currTraceInfo = &traceInfo;
        //    PreOpWriteTraceInfo(traceInfo, m_cpu->Registers(), *m_memoryBus);
        //}

        cycles_t cpuCycles = 0;

        // In case exception is thrown below, we still want to add the current instruction trace
        // info, so wrap the call in a ScopedExit
        //auto onExit = MakeScopedExit([&] {
        //    if (m_traceEnabled) {

        //        // If the CPU didn't do anything (e.g. waiting for interrupts), we have nothing
        //        // to log or hash
        //        Trace::InstructionTraceInfo lastTraceInfo;
        //        if (m_instructionTraceBuffer.PeekBack(lastTraceInfo)) {
        //            if (lastTraceInfo.postOpCpuRegisters.PC == m_cpu->Registers().PC) {
        //                m_currTraceInfo = nullptr;
        //                return;
        //            }
        //        }

        //        PostOpWriteTraceInfo(traceInfo, m_cpu->Registers(), cpuCycles);
        //        m_instructionTraceBuffer.PushBackMoveFront(traceInfo);
        //        m_currTraceInfo = nullptr;

        //        // Compute running hash of instruction trace
        //        if (!m_syncProtocol.IsStandalone())
        //            m_instructionHash = HashTraceInfo(traceInfo, m_instructionHash);

        //        ++m_numInstructionsExecutedThisFrame;
        //    }
        //});

        cpuCycles = m_emulator->ExecuteInstruction(input, renderContext, audioContext);
        return cpuCycles;

    } catch (std::exception& ex) {
        Printf("Exception caught:\n%s\n", ex.what());
        //PrintLastOp();
    } catch (...) {
        Printf("Unknown exception caught\n");
        //PrintLastOp();
    }
    
    //TODO!!!!!!!!!!!!!!!!!!!!!!!!!!!! Send interrupt signal and break
    //BreakIntoDebugger();

    return static_cast<cycles_t>(0);
};
