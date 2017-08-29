#include "Debugger.h"
#include "Cpu.h"
#include "CpuHelpers.h"
#include "CpuOpCodes.h"
#include "MemoryBus.h"
#include "Platform.h"
#include "RegexHelpers.h"
#include "StringHelpers.h"
#include <array>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

namespace {

    template <typename T>
    T HexStringToIntegral(const char* s) {
        std::stringstream converter(s);
        int64_t value;
        converter >> std::hex >> value;
        return static_cast<T>(value);
    }

    template <typename T>
    T HexStringToIntegral(const std::string& s) {
        return HexStringToIntegral<T>(s.c_str());
    }

    template <typename T>
    T StringToIntegral(std::string s) {
        if (s.length() == 0)
            return 0;

        if (s[0] == '$') { // '$' Hex value
            return HexStringToIntegral<T>(s.substr(1));
        } else if (s[0] == '0' && s[1] == 'x' || s[1] == 'X') { // '0x' Hex value
            return HexStringToIntegral<T>(s);
        } else { // Integral value
            int64_t value;
            std::stringstream converter(s);
            converter >> value;
            return static_cast<T>(value);
        }
    }

    std::vector<std::string> Tokenize(const std::string& s) { return Split(s, " \t"); }

    std::string TryMemoryBusRead(const MemoryBus& memoryBus, uint16_t address) {
        try {
            uint8_t value = memoryBus.Read(address);
            return FormattedString<>("$%02x (%d)", value, value).Value();
        } catch (...) {
            return "INVALID_READ";
        }
    }

    const char* GetRegisterName(const CpuRegisters& cpuRegisters, const uint8_t& r) {
        ptrdiff_t offset =
            reinterpret_cast<const uint8_t*>(&r) - reinterpret_cast<const uint8_t*>(&cpuRegisters);
        switch (offset) {
        case offsetof(CpuRegisters, A):
            return "A";
        case offsetof(CpuRegisters, B):
            return "B";
        case offsetof(CpuRegisters, DP):
            return "DP";
        case offsetof(CpuRegisters, CC):
            return "CC";
        default:
            FAIL();
            return "INVALID";
        }
    };

    const char* GetRegisterName(const CpuRegisters& cpuRegisters, const uint16_t& r) {
        ptrdiff_t offset =
            reinterpret_cast<const uint8_t*>(&r) - reinterpret_cast<const uint8_t*>(&cpuRegisters);
        switch (offset) {
        case offsetof(CpuRegisters, X):
            return "X";
        case offsetof(CpuRegisters, Y):
            return "Y";
        case offsetof(CpuRegisters, U):
            return "U";
        case offsetof(CpuRegisters, S):
            return "S";
        case offsetof(CpuRegisters, PC):
            return "PC";
        case offsetof(CpuRegisters, D):
            return "D";
        default:
            FAIL();
            return "INVALID";
        }
    };

    struct Instruction {
        const CpuOp& cpuOp;
        int page;
        std::array<uint8_t, 3> operands;
    };

    Instruction ReadInstruction(uint16_t opAddr, const MemoryBus& memoryBus) {
        int cpuOpPage = 0;
        uint8_t opCodeByte = memoryBus.Read(opAddr++);
        if (IsOpCodePage1(opCodeByte)) {
            cpuOpPage = 1;
            opCodeByte = memoryBus.Read(opAddr++);
        } else if (IsOpCodePage2(opCodeByte)) {
            cpuOpPage = 2;
            opCodeByte = memoryBus.Read(opAddr++);
        }

        const auto& cpuOp = LookupCpuOpRuntime(cpuOpPage, opCodeByte);
        Instruction result = {cpuOp, cpuOpPage};

        // Always read max operand bytes, even if they're not used. We would want to only read as
        // many operands as stipulated by the opcode's size - 1 (or 2 if not on page 0), but
        // unfortunately, indexed instructions sometimes read an extra operand byte, determined
        // dynamically. So for disassembling purposes, we just read 3 bytes and call it a day.
        for (auto& operand : result.operands) {
            operand = memoryBus.Read(opAddr++);
        }
        return result;
    }

    void DisassembleOp_EXG_TFR(const Instruction& instruction, const CpuRegisters& cpuRegisters,
                               const MemoryBus& memoryBus, std::string& disasmInstruction,
                               std::string& comment) {
        (void)cpuRegisters;
        (void)memoryBus;
        (void)comment;

        const auto& cpuOp = instruction.cpuOp;
        ASSERT(cpuOp.addrMode == AddressingMode::Inherent);
        uint8_t postbyte = instruction.operands[0];
        uint8_t src = (postbyte >> 4) & 0b111;
        uint8_t dst = postbyte & 0b111;
        if (postbyte & BITS(3)) {
            const char* const regName[]{"A", "B", "CC", "DP"};
            disasmInstruction =
                FormattedString<>("%s %s,%s", cpuOp.name, regName[src], regName[dst]);
        } else {
            const char* const regName[]{"D", "X", "Y", "U", "S", "PC"};
            disasmInstruction =
                FormattedString<>("%s %s,%s", cpuOp.name, regName[src], regName[dst]);
        }
    }

    void DisassembleOp_PSH_PUL(const Instruction& instruction, const CpuRegisters& cpuRegisters,
                               const MemoryBus& memoryBus, std::string& disasmInstruction,
                               std::string& comment) {
        (void)cpuRegisters;
        (void)memoryBus;

        const auto& cpuOp = instruction.cpuOp;
        ASSERT(cpuOp.addrMode == AddressingMode::Immediate);
        auto value = instruction.operands[0];
        std::vector<std::string> registers;
        if (value & BITS(0))
            registers.push_back("CC");
        if (value & BITS(1))
            registers.push_back("A");
        if (value & BITS(2))
            registers.push_back("B");
        if (value & BITS(3))
            registers.push_back("DP");
        if (value & BITS(4))
            registers.push_back("X");
        if (value & BITS(5))
            registers.push_back("Y");
        if (value & BITS(6)) {
            registers.push_back(cpuOp.opCode < 0x36 ? "U" : "S");
        }
        if (value & BITS(7))
            registers.push_back("PC");

        disasmInstruction = FormattedString<>("%s %s", cpuOp.name, Join(registers, ",").c_str());
        comment = FormattedString<>("#$%02x (%d)", value, value);
    }

    void DisassembleIndexedInstruction(const Instruction& instruction,
                                       const CpuRegisters& cpuRegisters, const MemoryBus& memoryBus,
                                       std::string& disasmInstruction, std::string& comment) {
        auto RegisterSelect = [&cpuRegisters](uint8_t postbyte) -> const uint16_t& {
            switch ((postbyte >> 5) & 0b11) {
            case 0b00:
                return cpuRegisters.X;
            case 0b01:
                return cpuRegisters.Y;
            case 0b10:
                return cpuRegisters.U;
            default: // 0b11:
                return cpuRegisters.S;
            }
        };

        uint16_t EA = 0;
        uint8_t postbyte = instruction.operands[0];
        bool supportsIndirect = true;
        std::string operands;

        if ((postbyte & BITS(7)) == 0) // (+/- 4 bit offset),R
        {
            // postbyte is a 5 bit two's complement number we convert to 8 bit.
            // So if bit 4 is set (sign bit), we extend the sign bit by turning on bits 6,7,8;
            int8_t offset = postbyte & 0b0001'1111;
            if (postbyte & BITS(4))
                offset |= 0b1110'0000;
            auto& reg = RegisterSelect(postbyte);
            EA = reg + offset;
            supportsIndirect = false;

            operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
            comment = FormattedString<>("%d,$%04x", offset, reg);
        } else {
            switch (postbyte & 0b1111) {
            case 0b0000: { // ,R+
                auto& reg = RegisterSelect(postbyte);
                EA = reg;
                supportsIndirect = false;

                operands = FormattedString<>(",%s+", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>(",$%04x+", reg);
            } break;
            case 0b0001: { // ,R++
                auto& reg = RegisterSelect(postbyte);
                EA = reg;

                operands = FormattedString<>(",%s++", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>(",$%04x++", reg);
            } break;
            case 0b0010: { // ,-R
                auto& reg = RegisterSelect(postbyte);
                EA = reg - 1;
                supportsIndirect = false;

                operands = FormattedString<>(",-%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>(",-$%04x", reg);
            } break;
            case 0b0011: { // ,--R
                auto& reg = RegisterSelect(postbyte);
                EA = reg - 2;

                operands = FormattedString<>(",--%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>(",--$%04x", reg);
            } break;
            case 0b0100: { // ,R
                auto& reg = RegisterSelect(postbyte);
                EA = reg;

                operands = FormattedString<>(",%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>(",$%04x", reg);
            } break;
            case 0b0101: { // (+/- B),R
                auto& reg = RegisterSelect(postbyte);
                auto offset = S16(cpuRegisters.B);
                EA = reg + offset;

                operands = FormattedString<>("B,%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b0110: { // (+/- A),R
                auto& reg = RegisterSelect(postbyte);
                auto offset = S16(cpuRegisters.A);
                EA = reg + offset;

                operands = FormattedString<>("A,%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b0111:
                FAIL_MSG("Illegal");
                break;
            case 0b1000: { // (+/- 7 bit offset),R
                auto& reg = RegisterSelect(postbyte);
                uint8_t postbyte2 = instruction.operands[1];
                auto offset = S16(postbyte2);
                EA = reg + offset;

                operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1001: { // (+/- 15 bit offset),R
                uint8_t postbyte2 = instruction.operands[1];
                uint8_t postbyte3 = instruction.operands[2];
                auto& reg = RegisterSelect(postbyte);
                auto offset = CombineToS16(postbyte2, postbyte3);
                EA = reg + offset;

                operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1010:
                FAIL_MSG("Illegal");
                break;
            case 0b1011: { // (+/- D),R
                auto& reg = RegisterSelect(postbyte);
                auto offset = S16(cpuRegisters.D);
                EA = reg + offset;

                operands = FormattedString<>("D,%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1100: { // (+/- 7 bit offset),PC
                uint8_t postbyte2 = instruction.operands[1];
                auto offset = S16(postbyte2);
                EA = cpuRegisters.PC + offset;

                operands = FormattedString<>("%d,PC", offset);
                comment = FormattedString<>("%d,$%04x", offset, cpuRegisters.PC);
            } break;
            case 0b1101: { // (+/- 15 bit offset),PC
                uint8_t postbyte2 = instruction.operands[1];
                uint8_t postbyte3 = instruction.operands[2];
                auto offset = CombineToS16(postbyte2, postbyte3);
                EA = cpuRegisters.PC + offset;

                operands = FormattedString<>("%d,PC", offset);
                comment = FormattedString<>("%d,$%04x", offset, cpuRegisters.PC);
            } break;
            case 0b1110:
                FAIL_MSG("Illegal");
                break;
            case 0b1111: { // [address] (Indirect-only)
                uint8_t postbyte2 = instruction.operands[1];
                uint8_t postbyte3 = instruction.operands[2];
                EA = CombineToS16(postbyte2, postbyte3);
            } break;
            default:
                FAIL_MSG("Illegal");
                break;
            }
        }

        if (supportsIndirect && (postbyte & BITS(4))) {
            uint8_t msb = memoryBus.Read(EA);
            uint8_t lsb = memoryBus.Read(EA + 1);
            EA = CombineToU16(msb, lsb);
            operands = FormattedString<>("[$%04x]", EA);
        }

        disasmInstruction = std::string(instruction.cpuOp.name) + " " + operands;
        comment +=
            FormattedString<>(", EA = $%04x = %s", EA, TryMemoryBusRead(memoryBus, EA).c_str());
    }

    struct DisassembledOp {
        std::string hexInstruction;
        std::string disasmInstruction;
        std::string comment;
        std::string description;
    };

    DisassembledOp DisassembleOp(const CpuRegisters& cpuRegisters, const MemoryBus& memoryBus,
                                 const Debugger::SymbolTable& symbolTable) {
        uint16_t opAddr = cpuRegisters.PC;
        auto instruction = ReadInstruction(opAddr, memoryBus);
        const auto& cpuOp = instruction.cpuOp;

        std::string hexInstruction;
        // Output instruction in hex
        for (uint16_t i = 0; i < cpuOp.size; ++i)
            hexInstruction += FormattedString<>("%02x", memoryBus.Read(opAddr + i));

        std::string disasmInstruction, comment;

        // First see if we have instruction-specific handlers. These are for special cases where the
        // default addressing mode handlers don't give enough information.
        bool handled = true;
        switch (cpuOp.opCode) {
        case 0x1E: // EXG
        case 0x1F: // TFR
            DisassembleOp_EXG_TFR(instruction, cpuRegisters, memoryBus, disasmInstruction, comment);
            break;

        case 0x34: // PSHS
        case 0x35: // PULS
        case 0x36: // PSHU
        case 0x37: // PULU
            DisassembleOp_PSH_PUL(instruction, cpuRegisters, memoryBus, disasmInstruction, comment);
            break;

        default:
            handled = false;
        }

        // If no instruction-specific handler, we disassemble based on addressing mode.
        if (!handled) {
            switch (cpuOp.addrMode) {
            case AddressingMode::Inherent: {
                disasmInstruction = cpuOp.name;
            } break;

            case AddressingMode::Immediate: {
                if (cpuOp.size == 2) {
                    auto value = instruction.operands[0];
                    disasmInstruction = FormattedString<>("%s #$%02x", cpuOp.name, value);
                    comment = FormattedString<>("(%d)", value);
                } else {
                    auto value = CombineToU16(instruction.operands[0], instruction.operands[1]);
                    disasmInstruction = FormattedString<>("%s #$%04x", cpuOp.name, value);
                    comment = FormattedString<>("(%d)", value);
                }
            } break;

            case AddressingMode::Extended: {
                auto msb = instruction.operands[0];
                auto lsb = instruction.operands[1];
                uint16_t EA = CombineToU16(msb, lsb);
                uint8_t value = memoryBus.Read(EA);
                disasmInstruction = FormattedString<>("%s $%04x", cpuOp.name, EA);
                comment = FormattedString<>("$%02x (%d)", value, value);
            } break;

            case AddressingMode::Direct: {
                uint16_t EA = CombineToU16(cpuRegisters.DP, instruction.operands[0]);
                uint8_t value = memoryBus.Read(EA);
                disasmInstruction =
                    FormattedString<>("%s $%02x", cpuOp.name, instruction.operands[0]);
                comment = FormattedString<>("DP:(PC) = $%02x = $%02x (%d)", EA, value, value);
            } break;

            case AddressingMode::Indexed: {
                DisassembleIndexedInstruction(instruction, cpuRegisters, memoryBus,
                                              disasmInstruction, comment);
            } break;

            case AddressingMode::Relative: {
                // Branch instruction with 8 or 16 bit signed relative offset
                uint16_t nextPC = cpuRegisters.PC + cpuOp.size;
                if (cpuOp.size == 2) {
                    auto offset = static_cast<int8_t>(instruction.operands[0]);
                    disasmInstruction =
                        FormattedString<>("%s $%02x", cpuOp.name, U16(offset) & 0x00FF);
                    comment =
                        FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
                } else {
                    // Could be a long branch from page 0 (3 bytes) or page 1 (4 bytes)
                    ASSERT(cpuOp.size >= 3);
                    auto offset = static_cast<int16_t>(
                        CombineToU16(instruction.operands[0], instruction.operands[1]));
                    disasmInstruction = FormattedString<>("%s $%04x", cpuOp.name, offset);
                    comment =
                        FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
                }
            } break;

            case AddressingMode::Illegal: {
            case AddressingMode::Variant:
                FAIL_MSG("Unexpected addressing mode");
            } break;
            }
        }

        // Appends symbol names to known addresses
        auto AppendSymbols = [&symbolTable](const std::string& s) {
            if (!symbolTable.empty()) {
                auto AppendSymbol = [&symbolTable](const std::smatch& m) -> std::string {
                    std::string result = m.str(0);
                    uint16_t address = StringToIntegral<uint16_t>(m.str(0));

                    auto range = symbolTable.equal_range(address);
                    if (range.first != range.second) {
                        std::vector<std::string> symbols;
                        std::transform(range.first, range.second, std::back_inserter(symbols),
                                       [](auto& kvp) { return kvp.second; });

                        result += "{" + Join(symbols, "|") + "}";
                    }
                    return result;
                };

                std::regex re("\\$[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9]");
                return RegexReplace(s, re, AppendSymbol);
            }
            return s;
        };

        disasmInstruction = AppendSymbols(disasmInstruction);
        comment = AppendSymbols(comment);

        return {hexInstruction, disasmInstruction, comment, cpuOp.description};
    };

    void PrintOp(const CpuRegisters& cpuRegisters, const MemoryBus& memoryBus,
                 const Debugger::SymbolTable& symbolTable) {
        auto op = DisassembleOp(cpuRegisters, memoryBus, symbolTable);

        using namespace Platform;
        ScopedConsoleColor scc(ConsoleColor::Gray);
        printf("[$%04x] ", cpuRegisters.PC);
        SetConsoleColor(ConsoleColor::LightYellow);
        printf("%-10s ", op.hexInstruction.c_str());
        SetConsoleColor(ConsoleColor::LightAqua);
        printf("%-32s ", op.disasmInstruction.c_str());
        SetConsoleColor(ConsoleColor::LightGreen);
        printf("%-40s ", op.comment.c_str());
        SetConsoleColor(ConsoleColor::LightPurple);
        printf("%s", op.description.c_str());
        printf("\n");
    }

    void PrintRegisters(const CpuRegisters& cpuRegisters) {
        const auto& cc = cpuRegisters.CC;

        std::string CC =
            FormattedString<>("%c%c%c%c%c%c%c%c", cc.Entire ? 'E' : 'e',
                              cc.FastInterruptMask ? 'F' : 'f', cc.HalfCarry ? 'H' : 'h',
                              cc.InterruptMask ? 'I' : 'i', cc.Negative ? 'N' : 'n',
                              cc.Zero ? 'Z' : 'z', cc.Overflow ? 'V' : 'v', cc.Carry ? 'C' : 'c')
                .Value();

        const auto& r = cpuRegisters;
        printf("A=$%02x (%d) B=$%02x (%d) D=$%04x (%d) X=$%04x (%d) "
               "Y=$%04x (%d) U=$%04x S=$%04x DP=$%02x PC=$%04x CC=%s\n",
               r.A, r.A, r.B, r.B, r.D, r.D, r.X, r.X, r.Y, r.Y, r.U, r.S, r.DP, r.PC, CC.c_str());
    }

    void PrintHelp() {
        printf("s[tep] [count]          step instruction [count times]\n"
               "c[ontinue]              continue running\n"
               "u[ntil] <address>       run until address is reached\n"
               "info reg[isters]        display register values\n"
               "p[rint] <address>       display value add address\n"
               "set <address>=<value>   set value at address\n"
               "info break              display breakpoints\n"
               "b[reak] <address>       set instruction breakpoint at address\n"
               "[ |r|a]watch <address>  set write/read/both watchpoint at address\n"
               "delete <index>          delete breakpoint at index\n"
               "disable <index>         disable breakpoint at index\n"
               "enable <index>          enable breakpoint at index\n"
               "loadsymbols <file>      load file with symbol/address definitions\n"
               "trace                   toggle disassembly trace\n"
               "color                   toggle colored output (slow)\n"
               "q[uit]                  quit\n"
               "h[help]                 display this help text\n");
    }

    bool LoadUserSymbolsFile(const char* file, Debugger::SymbolTable& symbolTable) {
        std::ifstream fin(file);
        if (!fin)
            return false;

        std::string line;
        while (std::getline(fin, line)) {
            auto tokens = Tokenize(line);
            if (tokens.size() >= 3 &&
                ((tokens[1].find("EQU") != -1) || (tokens[1].find("equ") != -1))) {
                auto address = StringToIntegral<uint16_t>(tokens[2]);
                symbolTable.insert({address, tokens[0]});
            }
        }
        return true;
    }

    void SetColorEnabled(bool enabled) {
        Platform::SetConsoleColoringEnabled(enabled);
        if (enabled) {
            // For colored trace, we must disable buffering for it to work (slow)
            setvbuf(stdout, NULL, _IONBF, 0);
        } else {
            // With color disabled, we can now buffer output in large chunks
            setvbuf(stdout, NULL, _IOFBF, 100 * 1024);
        }
    }
} // namespace

void Debugger::Init(MemoryBus& memoryBus, Cpu& cpu) {
    m_memoryBus = &memoryBus;
    m_cpu = &cpu;

    Platform::SetConsoleCtrlHandler([this] {
        m_breakIntoDebugger = true;
        return true;
    });

    SetColorEnabled(m_colorEnabled);

    m_lastCommand = "step"; // Reasonable default

    // Break on start
    m_breakIntoDebugger = true;

    // Enable trace when running normally
    m_traceEnabled = true;

    m_memoryBus->RegisterCallbacks(
        [&](uint16_t address) {
            if (auto bp = m_breakpoints.Get(address)) {
                if (bp->enabled && (bp->type == Breakpoint::Type::Read ||
                                    bp->type == Breakpoint::Type::ReadWrite)) {
                    m_breakIntoDebugger = true;
                    printf("Watchpoint hit at $%04x (read)\n", address);
                }
            }
        },
        [&](uint16_t address, uint8_t value) {
            if (auto bp = m_breakpoints.Get(address)) {
                if (bp->enabled && (bp->type == Breakpoint::Type::Write ||
                                    bp->type == Breakpoint::Type::ReadWrite)) {
                    m_breakIntoDebugger = true;
                    printf("Watchpoint hit at $%04x (write value $%02x)\n", address, value);
                }
            }
        });
}

bool Debugger::Update(double deltaTime) {
    auto PrintOp = [&] {
        if (m_traceEnabled) {
            m_memoryBus->SetCallbacksEnabled(false); // Don't stop on watchpoints when disassembling
            ::PrintOp(m_cpu->Registers(), *m_memoryBus, m_symbolTable);
            m_memoryBus->SetCallbacksEnabled(true);
        }
    };

    auto ExecuteInstruction = [&] {
        ++m_instructionCount;
        try {
            return m_cpu->ExecuteInstruction();
        } catch (std::exception& ex) {
            printf("Exception caught:\n%s\n", ex.what());
        } catch (...) {
            printf("Unknown exception caught\n");
        }
        m_breakIntoDebugger = true;
        return static_cast<cycles_t>(0);
    };

    // Set default console colors
    Platform::ScopedConsoleColor defaultColor(Platform::ConsoleColor::White,
                                              Platform::ConsoleColor::Black);

    if (m_breakIntoDebugger) {
        auto ContinueExecution = [&] {
            m_breakIntoDebugger = false;
        };

        printf("$%04x (%s)>", m_cpu->Registers().PC, m_lastCommand.c_str());

        std::string input;
        const auto& stream = std::getline(std::cin, input);

        Platform::ScopedConsoleColor defaultOutputColor(Platform::ConsoleColor::LightAqua);

        if (!stream) {
            // getline will fail under certain conditions, like when Ctrl+C is pressed, in which
            // case we just clear the stream status and restart the loop.
            std::cin.clear();
            std::cout << std::endl;
            return true;
        }

        auto tokens = Tokenize(input);

        // If no input, repeat last command
        if (tokens.size() == 0) {
            input = m_lastCommand;
            tokens = Tokenize(m_lastCommand);
        }

        bool validCommand = true;

        if (tokens.size() == 0) {
            // Don't do anything (no command entered yet)

        } else if (tokens[0] == "quit" || tokens[0] == "q") {
            return false;

        } else if (tokens[0] == "help" || tokens[0] == "h") {
            PrintHelp();

        } else if (tokens[0] == "continue" || tokens[0] == "c") {
            // First 'step' current instruction, otherwise if we have a breakpoint on it we will
            // end up breaking immediately on it again (we won't actually continue)
            PrintOp();
            ExecuteInstruction();
            ContinueExecution();

        } else if (tokens[0] == "step" || tokens[0] == "s") {
            // "Step into"
            PrintOp();
            ExecuteInstruction();

            // Handle optional number of steps parameter
            if (tokens.size() > 1) {
                m_numInstructionsToExecute = StringToIntegral<int64_t>(tokens[1]) - 1;
                if (m_numInstructionsToExecute.value() > 0) {
                    ContinueExecution();
                }
            }

        } else if (tokens[0] == "until" || tokens[0] == "u") {
            if (tokens.size() > 1) {
                uint16_t address = StringToIntegral<uint16_t>(tokens[1]);
                auto bp = m_breakpoints.Add(Breakpoint::Type::Instruction, address);
                bp->autoDelete = true;
                ContinueExecution();
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "break" || tokens[0] == "b") {
            validCommand = false;
            if (tokens.size() > 1) {
                uint16_t address = StringToIntegral<uint16_t>(tokens[1]);
                if (auto bp = m_breakpoints.Add(Breakpoint::Type::Instruction, address)) {
                    printf("Added breakpoint at $%04x\n", address);
                    validCommand = true;
                }
            }

        } else if (tokens[0] == "watch" || tokens[0] == "rwatch" || tokens[0] == "awatch") {
            validCommand = false;
            if (tokens.size() > 1) {
                uint16_t address = StringToIntegral<uint16_t>(tokens[1]);

                auto type = tokens[0][0] == 'w' ? Breakpoint::Type::Write
                                                : tokens[0][0] == 'r' ? Breakpoint::Type::Read
                                                                      : Breakpoint::Type::ReadWrite;

                if (auto bp = m_breakpoints.Add(type, address)) {
                    printf("Added watchpoint at $%04x\n", address);
                    validCommand = true;
                }
            }

        } else if (tokens[0] == "delete") {
            validCommand = false;
            if (tokens.size() > 1) {
                int breakpointIndex = std::stoi(tokens[1]);
                if (auto bp = m_breakpoints.RemoveAtIndex(breakpointIndex)) {
                    printf("Deleted breakpoint %d at $%04x\n", breakpointIndex, bp->address);
                    validCommand = true;
                } else {
                    printf("Invalid breakpoint specified\n");
                }
            }

        } else if (tokens[0] == "enable") {
            validCommand = false;
            if (tokens.size() > 1) {
                size_t breakpointIndex = std::stoi(tokens[1]);
                if (auto bp = m_breakpoints.GetAtIndex(breakpointIndex)) {
                    bp->enabled = true;
                    printf("Enabled breakpoint %d at $%04x\n", breakpointIndex, bp->address);
                    validCommand = true;
                } else {
                    printf("Invalid breakpoint specified\n");
                }
            }

        } else if (tokens[0] == "disable") {
            validCommand = false;
            if (tokens.size() > 1) {
                size_t breakpointIndex = std::stoi(tokens[1]);
                if (auto bp = m_breakpoints.GetAtIndex(breakpointIndex)) {
                    bp->enabled = false;
                    printf("Disabled breakpoint %d at $%04x\n", breakpointIndex, bp->address);
                    validCommand = true;
                } else {
                    printf("Invalid breakpoint specified\n");
                }
            }

        } else if (tokens[0] == "info") {
            if (tokens.size() > 1 && (tokens[1] == "registers" || tokens[1] == "reg")) {
                PrintRegisters(m_cpu->Registers());
            } else if (tokens.size() > 1 && (tokens[1] == "break")) {
                printf("Breakpoints:\n");
                Platform::ScopedConsoleColor scc;
                for (size_t i = 0; i < m_breakpoints.Num(); ++i) {
                    auto bp = m_breakpoints.GetAtIndex(i);
                    Platform::SetConsoleColor(bp->enabled ? Platform::ConsoleColor::LightGreen
                                                          : Platform::ConsoleColor::LightRed);
                    printf("%3d: $%04x\t%-20s%s\n", i, bp->address,
                           Breakpoint::TypeToString(bp->type),
                           bp->enabled ? "Enabled" : "Disabled");
                }
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "print" || tokens[0] == "p") {
            if (tokens.size() > 1) {
                uint16_t address = StringToIntegral<uint16_t>(tokens[1]);
                uint8_t value = m_memoryBus->Read(address);
                printf("$%04x = $%02x (%d)\n", address, value, value);
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "set") { // e.g. set $addr=value
            validCommand = false;
            if (tokens.size() > 1) {
                // Recombine the tokens after 'set' into a string so we can split it on '='. We
                // have to do this because the user may have put whitespace around '='.
                auto assignment =
                    std::accumulate(tokens.begin() + 1, tokens.end(), std::string(""));
                auto args = Split(assignment, "=");
                if (args.size() == 2) {
                    auto address = StringToIntegral<uint16_t>(args[0]);
                    auto value = StringToIntegral<uint8_t>(args[1]);
                    m_memoryBus->Write(address, value);
                    validCommand = true;
                }
            }

        } else if (tokens[0] == "loadsymbols") {
            if (tokens.size() > 1 && LoadUserSymbolsFile(tokens[1].c_str(), m_symbolTable)) {
                printf("Loaded symbols from %s\n", tokens[1].c_str());
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "trace") {
            m_traceEnabled = !m_traceEnabled;
            printf("Trace %s\n", m_traceEnabled ? "enabled" : "disabled");

        } else if (tokens[0] == "color") {
            m_colorEnabled = !m_colorEnabled;
            SetColorEnabled(m_colorEnabled);
            printf("Color %s\n", m_colorEnabled ? "enabled" : "disabled");

        } else {
            validCommand = false;
        }

        if (validCommand) {
            m_lastCommand = input;
        } else {
            printf("Invalid command: %s\n", input.c_str());
        }

    } else { // Not broken into debugger (running)

        const double cpuHz = 6'000'000.0 / 4.0; // Frequency of the CPU (cycles/second)
        const cycles_t cpuCyclesThisFrame = cpuHz * deltaTime;

        // Execute as many instructions that can fit in this time slice (plus one more at most)
        m_cpuCyclesLeft += cpuCyclesThisFrame;
        while (m_cpuCyclesLeft > 0) {
            if (auto bp = m_breakpoints.Get(m_cpu->Registers().PC)) {
                if (bp->type == Breakpoint::Type::Instruction) {
                    if (bp->autoDelete) {
                        m_breakpoints.Remove(m_cpu->Registers().PC);
                        m_breakIntoDebugger = true;
                    } else if (bp->enabled) {
                        printf("Breakpoint hit at %04x\n", bp->address);
                        m_breakIntoDebugger = true;
                    }
                }
            }

            if (m_breakIntoDebugger) {
                m_cpuCyclesLeft = 0;
                break;
            }

            PrintOp();

            const auto elapsedCycles = ExecuteInstruction();
            m_cpuCyclesTotal += elapsedCycles;
            m_cpuCyclesLeft -= elapsedCycles;

            if (m_numInstructionsToExecute && (--m_numInstructionsToExecute.value() == 0)) {
                m_numInstructionsToExecute = {};
                m_breakIntoDebugger = true;
            }

            if (m_breakIntoDebugger) {
                m_cpuCyclesLeft = 0;
                break;
            }
        }
    }

    return true;
}
