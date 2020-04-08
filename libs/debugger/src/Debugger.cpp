#include "debugger/Debugger.h"
#include "core/ConsoleOutput.h"
#include "core/ErrorHandler.h"
#include "core/Platform.h"
#include "core/RegexUtil.h"
#include "core/Stream.h"
#include "core/StringUtil.h"
#include "debugger/DebuggerUtil.h"
#include "emulator/Cpu.h"
#include "emulator/CpuHelpers.h"
#include "emulator/CpuOpCodes.h"
#include "emulator/Emulator.h"
#include "emulator/MemoryBus.h"
#include "emulator/Ram.h"
#include "emulator/Via.h"
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
    struct ScopedConsoleCtrlHandler {
        template <typename Handler>
        ScopedConsoleCtrlHandler(Handler handler) {
            m_oldHandler = Platform::GetConsoleCtrlHandler();
            Platform::SetConsoleCtrlHandler(handler);
        }

        ~ScopedConsoleCtrlHandler() { Platform::SetConsoleCtrlHandler(m_oldHandler); }

    private:
        decltype(Platform::GetConsoleCtrlHandler()) m_oldHandler;
    };

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
        } else if ((s[0] == '0' && s[1] == 'x') || s[1] == 'X') { // '0x' Hex value
            return HexStringToIntegral<T>(s);
        } else { // Integral value
            int64_t value;
            std::stringstream converter(s);
            converter >> value;
            return static_cast<T>(value);
        }
    }

    std::vector<std::string> Tokenize(const std::string& s) { return StringUtil::Split(s, " \t"); }

    // E.g. Given 0xd001 returns "$d001{VIA_port_a}"
    std::string FormatAddress(uint16_t address, const Debugger::SymbolTable& symbolTable) {
        std::string result = FormattedString<>("$%04x", address).Value();

        auto range = symbolTable.equal_range(address);
        if (range.first != range.second) {
            std::vector<std::string> symbols;
            std::transform(range.first, range.second, std::back_inserter(symbols),
                           [](auto& kvp) { return kvp.second; });

            result += "{" + StringUtil::Join(symbols, "|") + "}";
        }
        return result;
    }

    // Finds all instances of addresses in s (e.g. "$d001") and appends symbol name to each
    // E.g. Given "Addrs: $d001 and $d000" returns "Addrs: $d001{VIA_port_a} and $d000{VIA_port_b}"
    std::string FormatAddresses(std::string_view s, const Debugger::SymbolTable& symbolTable) {
        if (!symbolTable.empty()) {
            auto AppendSymbol = [&symbolTable](const std::smatch& m) -> std::string {
                std::string result = m.str(0);
                auto address = StringToIntegral<uint16_t>(m.str(0));

                auto range = symbolTable.equal_range(address);
                if (range.first != range.second) {
                    std::vector<std::string> symbols;
                    std::transform(range.first, range.second, std::back_inserter(symbols),
                                   [](auto& kvp) { return kvp.second; });

                    result += "{" + StringUtil::Join(symbols, "|") + "}";
                }
                return result;
            };

            std::regex re("\\$[A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9][A-Fa-f0-9]");
            return RegexUtil::RegexReplace(s, re, AppendSymbol);
        }
        return std::string{s};
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

    void DisassembleOp_EXG_TFR(const Trace::Instruction& instruction,
                               const CpuRegisters& cpuRegisters, std::string& disasmInstruction,
                               std::string& comment) {
        (void)cpuRegisters;
        (void)comment;

        const auto& cpuOp = instruction.cpuOp;
        ASSERT(cpuOp->addrMode == AddressingMode::Inherent);
        uint8_t postbyte = instruction.GetOperand(0);
        uint8_t src = (postbyte >> 4) & 0b111;
        uint8_t dst = postbyte & 0b111;
        if (postbyte & BITS(3)) {
            const char* const regName[]{"A", "B", "CC", "DP"};
            disasmInstruction =
                FormattedString<>("%s %s,%s", cpuOp->name, regName[src], regName[dst]);
        } else {
            const char* const regName[]{"D", "X", "Y", "U", "S", "PC"};
            disasmInstruction =
                FormattedString<>("%s %s,%s", cpuOp->name, regName[src], regName[dst]);
        }
    }

    void DisassembleOp_PSH_PUL(const Trace::Instruction& instruction,
                               const CpuRegisters& cpuRegisters, std::string& disasmInstruction,
                               std::string& comment) {
        (void)cpuRegisters;

        const auto& cpuOp = instruction.cpuOp;
        ASSERT(cpuOp->addrMode == AddressingMode::Immediate);
        auto value = instruction.GetOperand(0);
        std::vector<std::string> registers;
        if (value & BITS(0))
            registers.emplace_back("CC");
        if (value & BITS(1))
            registers.emplace_back("A");
        if (value & BITS(2))
            registers.emplace_back("B");
        if (value & BITS(3))
            registers.emplace_back("DP");
        if (value & BITS(4))
            registers.emplace_back("X");
        if (value & BITS(5))
            registers.emplace_back("Y");
        if (value & BITS(6)) {
            registers.emplace_back(cpuOp->opCode < 0x36 ? "U" : "S");
        }
        if (value & BITS(7))
            registers.emplace_back("PC");

        disasmInstruction =
            FormattedString<>("%s %s", cpuOp->name, StringUtil::Join(registers, ",").c_str());
        comment = FormattedString<>("#$%02x (%d)", value, value);
    }

    void DisassembleIndexedInstruction(const Trace::Instruction& instruction,
                                       const CpuRegisters& cpuRegisters,
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
        uint8_t postbyte = instruction.GetOperand(0);
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
                comment = "Illegal indexed instruction post-byte";
                break;
            case 0b1000: { // (+/- 7 bit offset),R
                auto& reg = RegisterSelect(postbyte);
                uint8_t postbyte2 = instruction.GetOperand(1);
                auto offset = S16(postbyte2);
                EA = reg + offset;

                operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1001: { // (+/- 15 bit offset),R
                uint8_t postbyte2 = instruction.GetOperand(1);
                uint8_t postbyte3 = instruction.GetOperand(2);
                auto& reg = RegisterSelect(postbyte);
                auto offset = CombineToS16(postbyte2, postbyte3);
                EA = reg + offset;

                operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1010:
                comment = "Illegal indexed instruction post-byte";
                break;
            case 0b1011: { // (+/- D),R
                auto& reg = RegisterSelect(postbyte);
                auto offset = S16(cpuRegisters.D);
                EA = reg + offset;

                operands = FormattedString<>("D,%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1100: { // (+/- 7 bit offset),PC
                uint8_t postbyte2 = instruction.GetOperand(1);
                auto offset = S16(postbyte2);
                EA = cpuRegisters.PC + offset;

                operands = FormattedString<>("%d,PC", offset);
                comment = FormattedString<>("%d,$%04x", offset, cpuRegisters.PC);
            } break;
            case 0b1101: { // (+/- 15 bit offset),PC
                uint8_t postbyte2 = instruction.GetOperand(1);
                uint8_t postbyte3 = instruction.GetOperand(2);
                auto offset = CombineToS16(postbyte2, postbyte3);
                EA = cpuRegisters.PC + offset;

                operands = FormattedString<>("%d,PC", offset);
                comment = FormattedString<>("%d,$%04x", offset, cpuRegisters.PC);
            } break;
            case 0b1110:
                comment = "Illegal indexed instruction post-byte";
                break;
            case 0b1111: { // [address] (Indirect-only)
                uint8_t postbyte2 = instruction.GetOperand(1);
                uint8_t postbyte3 = instruction.GetOperand(2);
                EA = CombineToS16(postbyte2, postbyte3);
            } break;
            default:
                comment = "Illegal indexed instruction post-byte";
                break;
            }
        }

        if (supportsIndirect && (postbyte & BITS(4))) {
            operands = FormattedString<>("[$%04x]", EA);
        }

        disasmInstruction = std::string(instruction.cpuOp->name) + " " + operands;
    }

    struct DisassembledOp {
        std::string hexInstruction;
        std::string disasmInstruction;
        std::string comment;
        std::string description;
    };

    DisassembledOp DisassembleOp(const Trace::InstructionTraceInfo& traceInfo,
                                 const Debugger::SymbolTable& symbolTable) {
        const auto& instruction = traceInfo.instruction;
        const auto& cpuRegisters = traceInfo.preOpCpuRegisters;
        const auto& cpuOp = instruction.cpuOp;

        // Output instruction in hex
        std::string hexInstruction;
        for (uint16_t i = 0; i < cpuOp->size; ++i) {
            hexInstruction += FormattedString<>("%02x", instruction.opBytes[i]);
        }

        std::string disasmInstruction, comment;

        // First see if we have instruction-specific handlers. These are for special cases where the
        // default addressing mode handlers don't give enough information.
        bool handled = true;
        switch (cpuOp->opCode) {
        case 0x1E: // EXG
        case 0x1F: // TFR
            DisassembleOp_EXG_TFR(instruction, cpuRegisters, disasmInstruction, comment);
            break;

        case 0x34: // PSHS
        case 0x35: // PULS
        case 0x36: // PSHU
        case 0x37: // PULU
            DisassembleOp_PSH_PUL(instruction, cpuRegisters, disasmInstruction, comment);
            break;

        default:
            handled = false;
        }

        // If no instruction-specific handler, we disassemble based on addressing mode.
        if (!handled) {
            switch (cpuOp->addrMode) {
            case AddressingMode::Inherent: {
                disasmInstruction = cpuOp->name;
            } break;

            case AddressingMode::Immediate: {
                if (cpuOp->size == 2) {
                    auto value = instruction.GetOperand(0);
                    disasmInstruction = FormattedString<>("%s #$%02x", cpuOp->name, value);
                    comment = FormattedString<>("(%d)", value);
                } else {
                    auto value = CombineToU16(instruction.GetOperand(0), instruction.GetOperand(1));
                    disasmInstruction = FormattedString<>("%s #$%04x", cpuOp->name, value);
                    comment = FormattedString<>("(%d)", value);
                }
            } break;

            case AddressingMode::Extended: {
                auto msb = instruction.GetOperand(0);
                auto lsb = instruction.GetOperand(1);
                uint16_t EA = CombineToU16(msb, lsb);
                disasmInstruction = FormattedString<>("%s $%04x", cpuOp->name, EA);
            } break;

            case AddressingMode::Direct: {
                uint16_t EA = CombineToU16(cpuRegisters.DP, instruction.GetOperand(0));
                disasmInstruction =
                    FormattedString<>("%s $%02x", cpuOp->name, instruction.GetOperand(0));
                comment = FormattedString<>("DP:(PC) = $%02x", EA);
            } break;

            case AddressingMode::Indexed: {
                DisassembleIndexedInstruction(instruction, cpuRegisters, disasmInstruction,
                                              comment);
            } break;

            case AddressingMode::Relative: {
                // Branch instruction with 8 or 16 bit signed relative offset
                uint16_t nextPC = cpuRegisters.PC + cpuOp->size;
                if (cpuOp->size == 2) {
                    auto offset = static_cast<int8_t>(instruction.GetOperand(0));
                    disasmInstruction =
                        FormattedString<>("%s $%02x", cpuOp->name, U16(offset) & 0x00FF);
                    comment =
                        FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
                } else {
                    // Could be a long branch from page 0 (3 bytes) or page 1 (4 bytes)
                    ASSERT(cpuOp->size >= 3);
                    auto offset = static_cast<int16_t>(
                        CombineToU16(instruction.GetOperand(0), instruction.GetOperand(1)));
                    disasmInstruction = FormattedString<>("%s $%04x", cpuOp->name, offset);
                    comment =
                        FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
                }
            } break;

            case AddressingMode::Illegal: {
            case AddressingMode::Variant:
                comment = "Unexpected addressing mode";
            } break;
            }
        }

        // Append memory accesses to comment section (if any)
        {
            // Skip the opcode + operand bytes - @TODO: we probably shouldn't be storing these in
            // the first place
            const size_t skipBytes = traceInfo.instruction.cpuOp->size;
            const bool initialSpace = !comment.empty();
            for (size_t i = skipBytes; i < traceInfo.numMemoryAccesses; ++i) {
                auto& ma = traceInfo.memoryAccesses[i];
                const char* separator = i == skipBytes ? (initialSpace ? " " : "") : " ";
                comment += FormattedString<>("%s$%04x%s$%x", separator, ma.address,
                                             ma.read ? "->" : "<-", ma.value);
            }
        }

        disasmInstruction = FormatAddresses(disasmInstruction, symbolTable);
        comment = FormatAddresses(comment, symbolTable);

        return {hexInstruction, disasmInstruction, comment, cpuOp->description};
    };

    std::string GetCCString(const CpuRegisters& cpuRegisters) {
        const auto& cc = cpuRegisters.CC;
        return FormattedString<>("%c%c%c%c%c%c%c%c", cc.Entire ? 'E' : 'e',
                                 cc.FastInterruptMask ? 'F' : 'f', cc.HalfCarry ? 'H' : 'h',
                                 cc.InterruptMask ? 'I' : 'i', cc.Negative ? 'N' : 'n',
                                 cc.Zero ? 'Z' : 'z', cc.Overflow ? 'V' : 'v', cc.Carry ? 'C' : 'c')
            .Value();
    }

    void PrintRegisters(const CpuRegisters& cpuRegisters) {
        const auto& r = cpuRegisters;
        Printf("A=$%02x (%d) B=$%02x (%d) D=$%04x (%d) X=$%04x (%d) "
               "Y=$%04x (%d) U=$%04x S=$%04x DP=$%02x PC=$%04x CC=%s",
               r.A, r.A, r.B, r.B, r.D, r.D, r.X, r.X, r.Y, r.Y, r.U, r.S, r.DP, r.PC,
               GetCCString(cpuRegisters).c_str());
    }

    void PrintRegistersCompact(const CpuRegisters& cpuRegisters) {
        const auto& r = cpuRegisters;
        Printf("A$%02x|B$%02x|X$%04x|Y$%04x|U$%04x|S$%04x|DP$%02x|%s", r.A, r.B, r.X, r.Y, r.U, r.S,
               r.DP, GetCCString(cpuRegisters).c_str());
    }

    // Used to print the current instruction before we execute it
    void PrintPreOp(const Trace::InstructionTraceInfo& traceInfo,
                    const Debugger::SymbolTable& symbolTable) {
        auto op = DisassembleOp(traceInfo, symbolTable);

        using namespace Platform;
        ScopedConsoleColor scc(ConsoleColor::Gray);
        Printf("[$%04x] ", traceInfo.preOpCpuRegisters.PC);
        SetConsoleColor(ConsoleColor::LightYellow);
        Printf("%-10s ", op.hexInstruction.c_str());
        SetConsoleColor(ConsoleColor::LightAqua);
        Printf("%-32s ", op.disasmInstruction.c_str());
        SetConsoleColor(ConsoleColor::LightGreen);
        Printf("%-40s ", op.comment.c_str());
    }

    // Used to print the instruction just executed
    void PrintOp(const Trace::InstructionTraceInfo& traceInfo,
                 const Debugger::SymbolTable& symbolTable) {
        auto op = DisassembleOp(traceInfo, symbolTable);

        using namespace Platform;
        ScopedConsoleColor scc(ConsoleColor::Gray);
        Printf("[$%04x] ", traceInfo.preOpCpuRegisters.PC);
        SetConsoleColor(ConsoleColor::LightYellow);
        Printf("%-10s ", op.hexInstruction.c_str());
        SetConsoleColor(ConsoleColor::LightAqua);
        Printf("%-32s ", op.disasmInstruction.c_str());
        SetConsoleColor(ConsoleColor::LightGreen);
        Printf("%-40s ", op.comment.c_str());
        SetConsoleColor(ConsoleColor::LightPurple);
        Printf("%2llu ", traceInfo.elapsedCycles);
        PrintRegistersCompact(traceInfo.postOpCpuRegisters);
        Printf("\n");
    }

    void PrintHelp() {
        Printf("\n"
               "s[tep] [count]                       step into instruction [count] times\n"
               "next                                 step over instruction\n"
               "fin[ish]                             step out instruction\n"
               "c[ontinue]                           continue running\n"
               "u[ntil] <address>                    run until address is reached\n"
               "info reg[isters]                     display register values\n"
               "p[rint] <address>                    display value add address\n"
               "set <address>=<value>                set value at address\n"
               "bt|backtrace                         display backtrace (call stack)\n"
               "info break                           display breakpoints\n"
               "b[reak] <address>                    set instruction breakpoint at address\n"
               "[ |r|a]watch <address>               set write/read/both watchpoint at address\n"
               "delete {<index>|*}                   delete breakpoint at index\n"
               "disable {<index>|*}                  disable breakpoint at index\n"
               "enable {<index>|*}                   enable breakpoint at index or all if *\n"
               "loadsymbols <file>                   load file with symbol/address definitions\n"
               "toggle ...                           toggle input option\n"
               "  color                                colored output (slow)\n"
               "  trace                                disassembly trace\n"
               "option ...                           set option\n"
               "  errors {ignore|log|logonce|fail}     error policy\n"
               "t[race] ...                          display trace output\n"
               "  -n <num_lines>                       display num_lines worth\n"
               "  -f <file_name>                       output trace to file_name\n"
               "q[uit]                               quit\n"
               "h[elp]                               display this help text\n"
               "\n");
    }

    bool LoadUserSymbolsFile(const char* file, Debugger::SymbolTable& symbolTable) {
        std::ifstream fin(file);
        if (!fin)
            return false;

        const auto ext = fs::path{file}.extension();
        const bool isLstFile = ext == ".lst";
        const bool isAsmFile = ext == ".a09" || ext == ".asm";
        const bool isMapFile = ext == ".map";

        std::string line;
        while (std::getline(fin, line)) {
            auto tokens = Tokenize(line);
            if (isLstFile) {
                // AS09 Assembler for M6809 [1.42] lst file by Frank A. Kingswood
                // NOTE: not the same format as ASxxxx Assembler V05.11 (GCC6809)(Motorola 6809) by
                // Alan R. Baldwin

                // Format:
                // Abs_a_b : $f584          62852
                // OR
                if (tokens.size() >= 3 && tokens[1] == ":") {
                    auto address = StringToIntegral<uint16_t>(tokens[2]);
                    symbolTable.insert({address, tokens[0]});
                }
            } else if (isAsmFile) {
                // Hand-coded asm or assembled file

                // Format:
                // ScreenW equ 256
                // OR (using AVOCET ASM09):
                // REG0     EQU     $C800
                if (tokens.size() >= 3 &&
                    ((tokens[1].find("EQU") != -1) || (tokens[1].find("equ") != -1))) {
                    auto address = StringToIntegral<uint16_t>(tokens[2]);
                    symbolTable.insert({address, tokens[0]});
                }
            } else if (isMapFile) {
                // ASxxxx Linker V05.11 (GCC6809) .map file

                // Format:
                // C880  __ZN10VectorList17s_simpleAlloca   vector_list.cpp
                if (tokens.size() == 3 && fs::path{tokens[2]}.extension() == ".cpp") {
                    auto address = StringToIntegral<uint16_t>("$" + tokens[0]);
                    symbolTable.insert({address, tokens[1]});
                }
            } else {
                // Unknown file type
                return false;
            }
        }

        return true;
    }

    void SetColorEnabled(bool enabled) {
        Platform::SetConsoleColoringEnabled(enabled);
        if (enabled) {
            // For colored trace, we must disable buffering for it to work (slow)
            setvbuf(stdout, nullptr, _IONBF, 0);
        } else {
            // With color disabled, we can now buffer output in large chunks
            setvbuf(stdout, nullptr, _IOFBF, 100 * 1024);
        }
    }

    // Returns the address of instruction immediately following PC in memory. Note that if the
    // instruction at PC is a call, it does not return the call target location, but rather where
    // the call would return to.
    uint16_t GetNextInstructionAddress(uint16_t PC, MemoryBus& memoryBus) {
        auto instruction = Trace::ReadInstruction(PC, memoryBus);
        uint16_t nextPC = PC + instruction.cpuOp->size;
        return nextPC;
    }

} // namespace

void Debugger::Init(const std::vector<std::string_view>& args,
                    std::shared_ptr<IEngineService>& engineService, fs::path devDir,
                    Emulator& emulator) {
    m_engineService = engineService;

    if (contains(args, "-server"))
        m_syncProtocol.InitServer();
    else if (contains(args, "-client"))
        m_syncProtocol.InitClient();

    m_devDir = std::move(devDir);
    m_emulator = &emulator;
    m_memoryBus = &emulator.GetMemoryBus();
    m_cpu = &emulator.GetCpu();

    Platform::InitConsole();

    Platform::SetConsoleCtrlHandler([this] {
        BreakIntoDebugger();
        return true;
    });

    SetColorEnabled(m_colorEnabled);

    m_lastCommand = "step"; // Reasonable default

    // Break on start?
    m_breakIntoDebugger = false;

    // Enable trace by default?
    m_traceEnabled = true;

    m_memoryBus->RegisterCallbacks(
        // OnRead
        [&](uint16_t address, uint8_t value) {
            if (m_traceEnabled && m_currTraceInfo) {
                m_currTraceInfo->AddMemoryAccess(address, value, true);
            }

            if (auto bp = m_breakpoints.Get(address)) {
                if (bp->enabled && (bp->type == Breakpoint::Type::Read ||
                                    bp->type == Breakpoint::Type::ReadWrite)) {
                    BreakIntoDebugger();
                    Printf("Watchpoint hit at %s (read value $%02x)\n",
                           FormatAddress(address, m_symbolTable).c_str(), value);
                }
            }
        },
        // OnWrite
        [&](uint16_t address, uint8_t value) {
            if (m_traceEnabled && m_currTraceInfo) {
                m_currTraceInfo->AddMemoryAccess(address, value, false);
            }

            if (auto bp = m_breakpoints.Get(address)) {
                if (bp->enabled && (bp->type == Breakpoint::Type::Write ||
                                    bp->type == Breakpoint::Type::ReadWrite)) {
                    BreakIntoDebugger();
                    Printf("Watchpoint hit at %s (write value $%02x)\n",
                           FormatAddress(address, m_symbolTable).c_str(), value);
                }
            }
        });

    // Load up commands for debugger to execute on startup
    std::ifstream fin(m_devDir / "debugger_startup.txt");
    if (fin) {
        std::string command;
        while (std::getline(fin, command)) {
            if (!command.empty())
                m_pendingCommands.push(command);
        }
    }
}

void Debugger::Reset() {
    m_cpuCyclesLeft = 0;
    // We want to keep our breakpoints when resetting a game
    // m_breakpoints.Reset();
    m_cpuCyclesTotal = 0;
    m_cpuCyclesLeft = 0;
    m_instructionTraceBuffer.Clear();
    m_currTraceInfo = nullptr;
    m_callStack.Clear();

    // Force ram to zero when running sync protocol for determinism
    if (!m_syncProtocol.IsStandalone()) {
        m_emulator->GetRam().Zero();
    }
}

void Debugger::BreakIntoDebugger(bool switchFocus) {
    m_breakIntoDebugger = true;
    if (switchFocus)
        m_engineService->SetFocusConsole();
}

void Debugger::ResumeFromDebugger(bool switchFocus) {
    m_breakIntoDebugger = false;
    if (switchFocus)
        m_engineService->SetFocusMainWindow();
}

void Debugger::PrintOp(const Trace::InstructionTraceInfo& traceInfo) {
    if (m_traceEnabled) {
        ::PrintOp(traceInfo, m_symbolTable);
    }
};

void Debugger::PrintLastOp() {
    if (m_traceEnabled) {
        Trace::InstructionTraceInfo traceInfo;
        if (m_instructionTraceBuffer.PeekBack(traceInfo)) {
            PrintOp(traceInfo);
        }
    }
};

void Debugger::PrintCallStack() {

    size_t i = 0;
    const auto& frames = m_callStack.Frames();
    for (auto iter = frames.rbegin(); iter != frames.rend(); ++iter, ++i) {
        if (i == 0) {
            // Print frame 0 (current PC)
            auto currAddress = m_cpu->Registers().PC;
            auto frameAddress = iter->frameAddress;
            Printf("#%3d $%04x in %s\n", i, currAddress,
                   FormatAddress(frameAddress, m_symbolTable).c_str());

        } else {
            // Print frames 1..n
            auto currAddress = (iter - 1)->calleeAddress;
            auto frameAddress = iter->frameAddress;

            Printf("#%3d $%04x in %s\n", i, currAddress,
                   FormatAddress(frameAddress, m_symbolTable).c_str());
        }
    }
}

void Debugger::PostOpUpdateCallstack(const CpuRegisters& preOpRegisters) {
    DebuggerUtil::PostOpUpdateCallstack(m_callStack, preOpRegisters, *m_cpu, *m_memoryBus);
}

bool Debugger::FrameUpdate(double frameTime, const EmuEvents& emuEvents, const Input& inputArg,
                           RenderContext& renderContext, AudioContext& audioContext) {

    auto input = inputArg; // Copy input arg so we can modify it for sync protocol
    if (m_syncProtocol.IsServer()) {
        m_syncProtocol.Server_SendFrameStart(frameTime, input);
    } else if (m_syncProtocol.IsClient()) {
        m_syncProtocol.Client_RecvFrameStart(frameTime, input);
    }

    m_numInstructionsExecutedThisFrame = 0;

    for (auto& event : emuEvents) {
        if (std::holds_alternative<EmuEvent::BreakIntoDebugger>(event.type)) {
            BreakIntoDebugger();
            break;
        }
    }

    // Set default console colors
    Platform::ScopedConsoleColor defaultColor(Platform::ConsoleColor::White,
                                              Platform::ConsoleColor::Black);

    if (m_breakIntoDebugger || !m_pendingCommands.empty()) {

        Platform::ScopedConsoleColor defaultOutputColor(Platform::ConsoleColor::LightAqua);

        std::string inputCommand;

        if (!m_pendingCommands.empty()) {
            inputCommand = m_pendingCommands.front();
            m_pendingCommands.pop();
            Printf("%s\n", inputCommand.c_str());
            FlushStream(ConsoleStream::Output);

        } else {
            // Display current instruction as part of the prompt
            Printf("*");
            Trace::InstructionTraceInfo traceInfo;
            Trace::PreOpWriteTraceInfo(traceInfo, m_cpu->Registers(), *m_memoryBus);
            PrintPreOp(traceInfo, m_symbolTable);

            // Also display the last executed command
            auto prompt = FormattedString<>(" (%s)>", m_lastCommand.c_str());
            inputCommand = Platform::ConsoleReadLine(prompt);
        }

        auto tokens = Tokenize(inputCommand);

        // If no input, repeat last command
        if (tokens.size() == 0) {
            inputCommand = m_lastCommand;
            tokens = Tokenize(m_lastCommand);
        }

        bool validCommand = true;

        auto Step = [&] {
            // For step, erase the prompt line. This makes it easier to read the output from
            // multiple stepped lines, and allows us to output the current instruction at the prompt
            // without it having it printed twice as we step.
            Rewind(ConsoleStream::Output);
            ExecuteInstruction(input, renderContext, audioContext);
            PrintLastOp();
            CheckForBreakpoints();
        };

        auto Continue = [&] {
            ExecuteInstruction(input, renderContext, audioContext);
            ResumeFromDebugger();
        };

        if (tokens.size() == 0) {
            // Don't do anything (no command entered yet)

        } else if (tokens[0] == "quit" || tokens[0] == "q") {
            return false;

        } else if (tokens[0] == "help" || tokens[0] == "h") {
            PrintHelp();

        } else if (tokens[0] == "continue" || tokens[0] == "c") {
            Continue();

        } else if (tokens[0] == "step" || tokens[0] == "s") {
            if (tokens.size() > 1) {
                m_numInstructionsToExecute = StringToIntegral<int64_t>(tokens[1]) - 1;
            }

            if (m_numInstructionsToExecute > 0)
                Continue();
            else
                Step();

        } else if (tokens[0] == "next") {
            // If the instruction we're about to execute is a call, add a temporary conditional
            // breakpoint on when the callstack returns to its current size.
            const uint16_t PC = m_cpu->Registers().PC;
            if (DebuggerUtil::IsCall(PC, *m_memoryBus)) {
                size_t stackSizeAtCall = m_callStack.Frames().size();
                m_conditionalBreakpoints
                    .Add([this, stackSizeAtCall]() {
                        if (m_callStack.Frames().size() < stackSizeAtCall) {
                            Printf("Warning! Function did not return normally.");
                            return true;
                        }

                        return m_callStack.Frames().size() == stackSizeAtCall;
                    })
                    .Once();

                Continue();
            } else {
                Step();
            }

        } else if (tokens[0] == "finish" || tokens[0] == "fin") {
            // If the instruction we're about to execute is a call, add a temporary conditional
            // breakpoint on when the callstack stack is 1 less than its current size.
            if (auto calleeAddress = m_callStack.GetLastCalleeAddress()) {
                if (DebuggerUtil::IsCall(*calleeAddress, *m_memoryBus)) {
                    size_t stackSizeAtCall = m_callStack.Frames().size();
                    m_conditionalBreakpoints
                        .Add([this, stackSizeAtCall]() {
                            if (m_callStack.Frames().size() < stackSizeAtCall - 1) {
                                Printf("Warning! Function did not return normally.");
                                return true;
                            }

                            return m_callStack.Frames().size() == (stackSizeAtCall - 1);
                        })
                        .Once();

                    Continue();
                } else {
                    Step();
                }
            }

        } else if (tokens[0] == "until" || tokens[0] == "u") {
            if (tokens.size() > 1) {
                auto address = StringToIntegral<uint16_t>(tokens[1]);
                m_breakpoints.Add(Breakpoint::Type::Instruction, address).Once();
                Continue();
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "backtrace" || tokens[0] == "bt") {
            PrintCallStack();

        } else if (tokens[0] == "break" || tokens[0] == "b") {
            validCommand = false;
            if (tokens.size() > 1) {
                auto address = StringToIntegral<uint16_t>(tokens[1]);
                m_breakpoints.Add(Breakpoint::Type::Instruction, address);
                Printf("Added breakpoint at $%04x\n", address);
                validCommand = true;
            }

        } else if (tokens[0] == "watch" || tokens[0] == "rwatch" || tokens[0] == "awatch") {
            validCommand = false;
            if (tokens.size() > 1) {
                auto address = StringToIntegral<uint16_t>(tokens[1]);

                auto type = tokens[0][0] == 'w' ? Breakpoint::Type::Write
                                                : tokens[0][0] == 'r' ? Breakpoint::Type::Read
                                                                      : Breakpoint::Type::ReadWrite;

                m_breakpoints.Add(type, address);
                Printf("Added watchpoint at $%04x\n", address);
                validCommand = true;
            }

        } else if (tokens[0] == "delete") {
            validCommand = false;
            if (tokens.size() > 1 && tokens[1] == "*") {
                m_breakpoints.RemoveAll();
                Printf("Deleted all breakpoints\n");
                validCommand = true;
            } else if (tokens.size() > 1) {
                int breakpointIndex = std::stoi(tokens[1]);
                if (auto bp = m_breakpoints.RemoveAtIndex(breakpointIndex)) {
                    Printf("Deleted breakpoint %d at $%04x\n", breakpointIndex, bp->address);
                    validCommand = true;
                } else {
                    Printf("Invalid breakpoint specified\n");
                }
            }

        } else if (tokens[0] == "enable") {
            validCommand = false;
            if (tokens.size() > 1 && tokens[1] == "*") {
                for (size_t i = 0; i < m_breakpoints.Num(); ++i)
                    m_breakpoints.GetAtIndex(i)->enabled = true;
                Printf("Enabled all breakpoints\n");
                validCommand = true;
            } else if (tokens.size() > 1) {
                size_t breakpointIndex = std::stoi(tokens[1]);
                if (auto bp = m_breakpoints.GetAtIndex(breakpointIndex)) {
                    bp->enabled = true;
                    Printf("Enabled breakpoint %d at $%04x\n", breakpointIndex, bp->address);
                    validCommand = true;
                } else {
                    Printf("Invalid breakpoint specified\n");
                }
            }

        } else if (tokens[0] == "disable") {
            validCommand = false;
            if (tokens.size() > 1 && tokens[1] == "*") {
                for (size_t i = 0; i < m_breakpoints.Num(); ++i)
                    m_breakpoints.GetAtIndex(i)->enabled = false;
                Printf("Disabled all breakpoints\n");
                validCommand = true;
            } else if (tokens.size() > 1) {
                size_t breakpointIndex = std::stoi(tokens[1]);
                if (auto bp = m_breakpoints.GetAtIndex(breakpointIndex)) {
                    bp->enabled = false;
                    Printf("Disabled breakpoint %d at $%04x\n", breakpointIndex, bp->address);
                    validCommand = true;
                } else {
                    Printf("Invalid breakpoint specified\n");
                }
            }

        } else if (tokens[0] == "info") {
            if (tokens.size() > 1 && (tokens[1] == "registers" || tokens[1] == "reg")) {
                PrintRegisters(m_cpu->Registers());
                Printf("\n");
            } else if (tokens.size() > 1 && (tokens[1] == "break")) {
                Printf("Breakpoints:\n");
                Platform::ScopedConsoleColor scc;
                for (size_t i = 0; i < m_breakpoints.Num(); ++i) {
                    auto bp = m_breakpoints.GetAtIndex(i);
                    // TODO: Don't display "once" breakpoints (or add a "hidden" property?)
                    Platform::SetConsoleColor(bp->enabled ? Platform::ConsoleColor::LightGreen
                                                          : Platform::ConsoleColor::LightRed);
                    Printf("%3d: $%04x\t%-20s%s\n", i, bp->address,
                           Breakpoint::TypeToString(bp->type),
                           bp->enabled ? "Enabled" : "Disabled");
                }
                // TODO: display conditional breakpoints
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "print" || tokens[0] == "p") {
            if (tokens.size() > 1) {
                auto address = StringToIntegral<uint16_t>(tokens[1]);
                uint8_t value = m_memoryBus->ReadRaw(address);
                Printf("%s = $%02x (%d)\n", FormatAddress(address, m_symbolTable).c_str(), value,
                       value);
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
                auto args = StringUtil::Split(assignment, "=");
                if (args.size() == 2) {
                    auto address = StringToIntegral<uint16_t>(args[0]);
                    auto value = StringToIntegral<uint8_t>(args[1]);
                    m_memoryBus->Write(address, value);
                    validCommand = true;
                }
            }

        } else if (tokens[0] == "loadsymbols") {
            if (tokens.size() > 1 && LoadUserSymbolsFile(tokens[1].c_str(), m_symbolTable)) {
                Printf("Loaded symbols from %s\n", tokens[1].c_str());
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "toggle") {
            if (tokens.size() > 1) {
                if (tokens[1] == "color") {
                    m_colorEnabled = !m_colorEnabled;
                    SetColorEnabled(m_colorEnabled);
                    Printf("Color %s\n", m_colorEnabled ? "enabled" : "disabled");
                } else if (tokens[1] == "trace") {
                    m_traceEnabled = !m_traceEnabled;
                    Printf("Trace %s\n", m_traceEnabled ? "enabled" : "disabled");
                }
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "option") {
            if (tokens.size() > 2) {
                if (tokens[1] == "errors") {
                    if (tokens[2] == "ignore")
                        ErrorHandler::SetPolicy(ErrorHandler::Policy::Ignore);
                    else if (tokens[2] == "log")
                        ErrorHandler::SetPolicy(ErrorHandler::Policy::Log);
                    else if (tokens[2] == "logonce")
                        ErrorHandler::SetPolicy(ErrorHandler::Policy::LogOnce);
                    else if (tokens[2] == "fail")
                        ErrorHandler::SetPolicy(ErrorHandler::Policy::Fail);
                    else
                        validCommand = false;
                }
            } else {
                validCommand = false;
            }

        } else if (tokens[0] == "trace" || tokens[0] == "t") {
            size_t numLines = 10;
            const char* outFileName = nullptr;

            try {
                for (size_t i = 1; i < tokens.size(); ++i) {
                    auto& token = tokens[i];
                    if (token == "-n") {
                        numLines = StringToIntegral<size_t>(tokens.at(i + 1));
                        ++i;
                    } else if (token == "-f") {
                        outFileName = tokens.at(i + 1).c_str();
                        ++i;
                    } else {
                        throw std::exception();
                    }
                }
            } catch (...) {
                validCommand = false;
            }

            if (validCommand) {
                FileStream fileStream;
                ScopedOverridePrintStream ScopedOverridePrintStream;

                if (outFileName) {
                    fs::path outFilePath = m_devDir / outFileName;
                    if (!outFilePath.has_extension()) {
                        outFilePath.replace_extension(".txt");
                    }
                    if (!fileStream.Open(outFilePath, "w+"))
                        Printf("Failed to create trace file\n");
                    else {
                        Printf("Writing trace to \"%ws\"\n", fs::absolute(outFilePath).c_str());
                        ScopedOverridePrintStream.SetPrintStream(fileStream.Get());
                    }
                }

                // Allow Ctrl+C to break out of printing ops (can be very long)
                bool bKeepPrinting = true;
                auto scopedConsoleCtrlHandler = ScopedConsoleCtrlHandler([&bKeepPrinting] {
                    bKeepPrinting = false;
                    return true;
                });

                std::vector<Trace::InstructionTraceInfo> buffer(numLines);
                auto numInstructions = m_instructionTraceBuffer.PeekBack(buffer.data(), numLines);
                buffer.resize(numInstructions);
                Printf("\nTrace (last %d instructions):\n", numLines);
                for (auto& traceInfo : buffer) {
                    PrintOp(traceInfo);

                    if (!bKeepPrinting)
                        break;
                }
            }
        } else {
            validCommand = false;
        }

        if (validCommand) {
            m_lastCommand = inputCommand;
        } else {
            Printf("Invalid command: %s\n", inputCommand.c_str());
        }
    } else { // Not broken into debugger (running)

        ExecuteFrameInstructions(frameTime, input, renderContext, audioContext);
    }

    SyncInstructionHash(m_numInstructionsExecutedThisFrame);

    if (m_syncProtocol.IsServer()) {
        m_syncProtocol.Server_RecvFrameEnd();
    } else if (m_syncProtocol.IsClient()) {
        m_syncProtocol.Client_SendFrameEnd();
    }

    return true;
}

void Debugger::CheckForBreakpoints() {
    if (auto bp = m_breakpoints.Get(m_cpu->Registers().PC)) {
        if (bp->type == Breakpoint::Type::Instruction) {
            if (bp->once) {
                m_breakpoints.Remove(m_cpu->Registers().PC);
                BreakIntoDebugger();
            } else if (bp->enabled) {
                Printf("Breakpoint hit at %04x\n", bp->address);
                BreakIntoDebugger();
            }
        }
    }

    // Handle conditional breakpoints
    {
        bool shouldBreak = false;
        auto& conditionals = m_conditionalBreakpoints.Breakpoints();
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
            BreakIntoDebugger();
    }
}

void Debugger::ExecuteFrameInstructions(double frameTime, const Input& input,
                                        RenderContext& renderContext, AudioContext& audioContext) {
    // Execute as many instructions that can fit in this time slice (plus one more at most)
    const double cpuCyclesThisFrame = Cpu::Hz * frameTime;
    m_cpuCyclesLeft += cpuCyclesThisFrame;

    while (m_cpuCyclesLeft > 0) {
        CheckForBreakpoints();

        if (m_breakIntoDebugger) {
            m_cpuCyclesLeft = 0;
            break;
        }

        const cycles_t elapsedCycles = ExecuteInstruction(input, renderContext, audioContext);

        m_cpuCyclesTotal += elapsedCycles;
        m_cpuCyclesLeft -= elapsedCycles;

        if (m_numInstructionsToExecute && (--m_numInstructionsToExecute.value() == 0)) {
            m_numInstructionsToExecute = {};
            BreakIntoDebugger();
        }

        if (m_breakIntoDebugger) {
            m_cpuCyclesLeft = 0;
            break;
        }
    }
}

cycles_t Debugger::ExecuteInstruction(const Input& input, RenderContext& renderContext,
                                      AudioContext& audioContext) {
    try {
        Trace::InstructionTraceInfo traceInfo;
        if (m_traceEnabled) {
            m_currTraceInfo = &traceInfo;
            PreOpWriteTraceInfo(traceInfo, m_cpu->Registers(), *m_memoryBus);
        }

        cycles_t cpuCycles = 0;
        const auto preOpRegisters = m_cpu->Registers();

        // In case exception is thrown below, we still want to add the current instruction trace
        // info, so wrap the call in a ScopedExit
        auto onExit = MakeScopedExit([&] {
            PostOpUpdateCallstack(preOpRegisters);

            if (m_traceEnabled) {

                // If the CPU didn't do anything (e.g. waiting for interrupts), we have nothing
                // to log or hash
                Trace::InstructionTraceInfo lastTraceInfo;
                if (m_instructionTraceBuffer.PeekBack(lastTraceInfo)) {
                    if (lastTraceInfo.postOpCpuRegisters.PC == m_cpu->Registers().PC) {
                        m_currTraceInfo = nullptr;
                        return;
                    }
                }

                PostOpWriteTraceInfo(traceInfo, m_cpu->Registers(), cpuCycles);
                m_instructionTraceBuffer.PushBackMoveFront(traceInfo);
                m_currTraceInfo = nullptr;

                // Compute running hash of instruction trace
                if (!m_syncProtocol.IsStandalone())
                    m_instructionHash = HashTraceInfo(traceInfo, m_instructionHash);

                ++m_numInstructionsExecutedThisFrame;
            }
        });

        cpuCycles = m_emulator->ExecuteInstruction(input, renderContext, audioContext);
        return cpuCycles;

    } catch (std::exception& ex) {
        Printf("Exception caught:\n%s\n", ex.what());
        PrintLastOp();
    } catch (...) {
        Printf("Unknown exception caught\n");
        PrintLastOp();
    }
    BreakIntoDebugger();
    return static_cast<cycles_t>(0);
};

void Debugger::SyncInstructionHash(int numInstructionsExecutedThisFrame) {
    if (m_syncProtocol.IsStandalone())
        return;

    bool hashMismatch = false;

    // Sync hashes and compare
    if (m_syncProtocol.IsServer()) {
        m_syncProtocol.SendValue(ConnectionType::Server, m_instructionHash);

    } else if (m_syncProtocol.IsClient()) {
        uint32_t serverInstructionHash{};
        m_syncProtocol.RecvValue(ConnectionType::Client, serverInstructionHash);
        hashMismatch = m_instructionHash != serverInstructionHash;
    }

    // Sync whether to continue or stop
    if (m_syncProtocol.IsClient()) {
        m_syncProtocol.SendValue(ConnectionType::Client, hashMismatch);
    } else if (m_syncProtocol.IsServer()) {
        m_syncProtocol.RecvValue(ConnectionType::Server, hashMismatch);
    }

    if (hashMismatch) {
        Errorf("Instruction hash mismatch in last %d instructions\n",
               numInstructionsExecutedThisFrame);

        // @TODO: Unfortunately, we still deadlock when multiple instances call BreakIntoDebugger at
        // the same time, so for now, just don't do it.
        // BreakIntoDebugger();
        m_breakIntoDebugger = true;

        if (m_syncProtocol.IsServer())
            m_syncProtocol.ShutdownServer();
        else
            m_syncProtocol.ShutdownClient();
    }
}
