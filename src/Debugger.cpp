#include "Debugger.h"
#include "Cpu.h"
#include "CpuHelpers.h"
#include "CpuOpCodes.h"
#include "MemoryBus.h"
#include "Platform.h"
#include "RegexHelpers.h"
#include "StringHelpers.h"
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {
    struct Breakpoint {
        uint16_t address = 0;
        bool enabled = true;
        bool autoDelete = false;
    };

    class Breakpoints {
    public:
        Breakpoint* Add(uint16_t address) {
            auto& bp = m_breakpoints[address];
            bp.address = address;
            return &bp;
        }

        std::optional<Breakpoint> Remove(uint16_t address) {
            auto iter = m_breakpoints.find(address);
            if (iter != m_breakpoints.end()) {
                auto bp = iter->second;
                m_breakpoints.erase(iter);
                return bp;
            }
            return {};
        }

        std::optional<Breakpoint> RemoveAtIndex(size_t index) {
            auto iter = GetBreakpointIterAtIndex(index);
            if (iter != m_breakpoints.end()) {
                auto bp = iter->second;
                m_breakpoints.erase(iter);
                return bp;
            }
            return {};
        }

        Breakpoint* Get(uint16_t address) {
            auto iter = m_breakpoints.find(address);
            if (iter != m_breakpoints.end()) {
                return &iter->second;
            }
            return nullptr;
        }

        Breakpoint* GetAtIndex(size_t index) {
            auto iter = GetBreakpointIterAtIndex(index);
            if (iter != m_breakpoints.end()) {
                return &iter->second;
            }
            return nullptr;
        }

        std::optional<size_t> GetIndex(uint16_t address) {
            auto iter = m_breakpoints.find(address);
            if (iter != m_breakpoints.end()) {
                return std::distance(m_breakpoints.begin(), iter);
            }
            return {};
        }

        size_t Num() const { return m_breakpoints.size(); }

    private:
        std::map<uint16_t, Breakpoint> m_breakpoints;

        using IterType = decltype(m_breakpoints.begin());
        IterType GetBreakpointIterAtIndex(size_t index) {
            auto iter = m_breakpoints.begin();
            if (iter != m_breakpoints.end())
                std::advance(iter, index);
            return iter;
        }
    };

    template <typename T>
    T HexStringToIntegral(const char* s) {
        std::stringstream converter(s);
        T value;
        converter >> std::hex >> value;
        return value;
    }

    std::vector<std::string> Tokenize(const std::string& s) { return Split(s, " \t"); }

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
            assert(false);
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
            assert(false);
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
        assert(cpuOp.addrMode == AddressingMode::Inherent);
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
        assert(cpuOp.addrMode == AddressingMode::Immediate);
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
            uint8_t offset = postbyte & 0b0000'1111;
            if (postbyte & BITS(4))
                offset |= 0b1110'0000;
            auto& reg = RegisterSelect(postbyte);
            EA = reg + S16(offset);
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
                FAIL("Illegal");
                break;
            case 0b1000: { // (+/- 7 bit offset),R
                auto& reg = RegisterSelect(postbyte);
                uint8_t postbyte2 = instruction.operands[2];
                auto offset = S16(postbyte2);
                EA = reg + offset;

                operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1001: { // (+/- 15 bit offset),R
                uint8_t postbyte2 = instruction.operands[2];
                uint8_t postbyte3 = instruction.operands[3];
                auto& reg = RegisterSelect(postbyte);
                auto offset = CombineToS16(postbyte2, postbyte3);
                EA = reg + offset;

                operands = FormattedString<>("%d,%s", offset, GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1010:
                FAIL("Illegal");
                break;
            case 0b1011: { // (+/- D),R
                auto& reg = RegisterSelect(postbyte);
                auto offset = S16(cpuRegisters.D);
                EA = reg + offset;

                operands = FormattedString<>("D,%s", GetRegisterName(cpuRegisters, reg));
                comment = FormattedString<>("%d,$%04x", offset, reg);
            } break;
            case 0b1100: { // (+/- 7 bit offset),PC
                uint8_t postbyte2 = instruction.operands[2];
                auto offset = S16(postbyte2);
                EA = cpuRegisters.PC + offset;

                operands = FormattedString<>("%d,PC", offset);
                comment = FormattedString<>("%d,$%04x", offset, cpuRegisters.PC);
            } break;
            case 0b1101: { // (+/- 15 bit offset),PC
                uint8_t postbyte2 = instruction.operands[2];
                uint8_t postbyte3 = instruction.operands[3];
                auto offset = CombineToS16(postbyte2, postbyte3);
                EA = cpuRegisters.PC + offset;

                operands = FormattedString<>("%d,PC", offset);
                comment = FormattedString<>("%d,$%04x", offset, cpuRegisters.PC);
            } break;
            case 0b1110:
                FAIL("Illegal");
                break;
            case 0b1111: { // [address] (Indirect-only)
                uint8_t postbyte2 = instruction.operands[2];
                uint8_t postbyte3 = instruction.operands[3];
                EA = CombineToS16(postbyte2, postbyte3);
            } break;
            default:
                FAIL("Illegal");
                break;
            }
        }

        if (supportsIndirect && (postbyte & BITS(4))) {
            uint8_t msb = memoryBus.Read(EA);
            uint8_t lsb = memoryBus.Read(EA + 1);
            EA = CombineToU16(lsb, msb);
            operands = "[" + operands + "]";
        }

        disasmInstruction = std::string(instruction.cpuOp.name) + " " + operands;
        uint8_t value = memoryBus.Read(EA);
        comment += FormattedString<>(", EA = $%04x = $%02x (%d)", EA, value, value);
    }

    std::string DisassembleOp(const CpuRegisters& cpuRegisters, const MemoryBus& memoryBus,
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
                auto nextPC = cpuRegisters.PC + cpuOp.size;
                if (cpuOp.size == 2) {
                    auto offset = static_cast<int8_t>(instruction.operands[0]);
                    disasmInstruction =
                        FormattedString<>("%s $%02x", cpuOp.name, U16(offset) & 0x00FF);
                    comment =
                        FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
                } else {
                    assert(cpuOp.size == 3);
                    auto offset = static_cast<int16_t>(
                        CombineToU16(instruction.operands[0], instruction.operands[1]));
                    disasmInstruction = FormattedString<>("%s $%04x", cpuOp.name, offset);
                    comment =
                        FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
                }
            } break;

            case AddressingMode::Illegal: {
            case AddressingMode::Variant:
                assert(false);
            } break;
            }
        }

        // Appends symbol names to known addresses
        auto AppendSymbols = [&symbolTable](const std::string& s) {
            if (!symbolTable.empty()) {
                auto AppendSymbol = [&symbolTable](const std::smatch& m) -> std::string {
                    std::string result = m.str(0);
                    uint16_t address = HexStringToIntegral<uint16_t>(m.str(0).substr(1).c_str());
                    auto iter = symbolTable.find(address);
                    if (iter != symbolTable.end()) {
                        result += "{" + iter->second + "}";
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

        std::string result =
            FormattedString<>("%-10s %-32s %s", hexInstruction.c_str(), disasmInstruction.c_str(),
                              comment.size() > 0 ? ("; " + comment).c_str() : "")
                .Value();

        return result;
    };

    void PrintOp(const CpuRegisters& cpuRegisters, const MemoryBus& memoryBus,
                 const Debugger::SymbolTable& symbolTable) {
        std::string op = DisassembleOp(cpuRegisters, memoryBus, symbolTable);
        std::cout << FormattedString<>("[$%x] %s", cpuRegisters.PC, op.c_str()) << std::endl;
    }

    void PrintRegisters(const CpuRegisters& cpuRegisters) {
        const auto& cc = cpuRegisters.CC;

        std::string CC =
            FormattedString<>("%c%c%c%c%c%c%c%c", cc.Carry ? 'C' : 'c', cc.Overflow ? 'V' : 'v',
                              cc.Zero ? 'Z' : 'z', cc.Negative ? 'N' : 'n',
                              cc.InterruptMask ? 'I' : 'i', cc.HalfCarry ? 'H' : 'h',
                              cc.FastInterruptMask ? 'F' : 'f', cc.Entire ? 'E' : 'e')
                .Value();

        const auto& r = cpuRegisters;
        std::cout << FormattedString<>("A=$%02x (%d) B=$%02x (%d) D=$%04x (%d) X=$%04x (%d) "
                                       "Y=$%04x (%d) U=$%04x S=$%04x DP=$%02x PC=$%04x CC=%s",
                                       r.A, r.A, r.B, r.B, r.D, r.D, r.X, r.X, r.Y, r.Y, r.U, r.S,
                                       r.DP, r.PC, CC.c_str())
                  << std::endl;
    }

    void PrintHelp() {
        std::cout << "s[tep]                step instruction\n"
                     "c[ontinue]            continue running\n"
                     "u[ntil] <address>     run until address is reached\n"
                     "info reg[isters]      display register values\n"
                     "p[rint] <address>     display value add address\n"
                     "info break            display breakpoints\n"
                     "b[reak] <address>     set breakpoint at address\n"
                     "delete <index>        delete breakpoint at index\n"
                     "disable <index>       disable breakpoint at index\n"
                     "enable <index>        enable breakpoint at index\n"
                     "loadsymbols <file>    load file with symbol/address definitions\n"
                     "q[uit]                quit\n"
                     "h[help]               display this help text\n"
                  << std::flush;
    }

    bool LoadUserSymbolsFile(const char* file, Debugger::SymbolTable& symbolTable) {
        std::ifstream fin(file);
        if (!fin)
            return false;

        std::string line;
        while (std::getline(fin, line)) {
            auto tokens = Tokenize(line);
            if (tokens.size() >= 3 && tokens[1] == ".EQU") {
                auto address = HexStringToIntegral<uint16_t>(tokens[2].c_str());
                symbolTable[address] = tokens[0];
            }
        }
        return true;
    }

} // namespace

void Debugger::Init(MemoryBus& memoryBus, Cpu& cpu) {
    m_memoryBus = &memoryBus;
    m_cpu = &cpu;

    Platform::SetConsoleCtrlHandler([this] {
        m_breakIntoDebugger = true;
        return true;
    });
}

void Debugger::Run() {
    m_lastCommand = "step"; // Reasonable default

    // Break on start
    m_breakIntoDebugger = true;

    // Enable trace when running normally
    m_traceEnabled = true;

    Breakpoints breakpoints;

    while (true) {
        if (m_breakIntoDebugger) {
            std::cout << FormattedString<>("$%04x (%s)>", m_cpu->Registers().PC,
                                           m_lastCommand.c_str())
                      << std::flush;

            std::string input;
            const auto& stream = std::getline(std::cin, input);

            if (!stream) {
                // getline will fail under certain conditions, like when Ctrl+C is pressed, in which
                // case we just clear the stream status and restart the loop.
                std::cin.clear();
                std::cout << std::endl;
                continue;
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
                return;

            } else if (tokens[0] == "help" || tokens[0] == "h") {
                PrintHelp();

            } else if (tokens[0] == "continue" || tokens[0] == "c") {
                m_breakIntoDebugger = false;

            } else if (tokens[0] == "step" || tokens[0] == "s") {
                // "Step into"
                PrintOp(m_cpu->Registers(), *m_memoryBus, m_symbolTable);
                m_cpu->ExecuteInstruction();

            } else if (tokens[0] == "until" || tokens[0] == "u") {
                if (tokens.size() > 1 && tokens[1][0] == '$') {
                    uint16_t address = HexStringToIntegral<uint16_t>(tokens[1].substr(1).c_str());
                    auto bp = breakpoints.Add(address);
                    bp->autoDelete = true;
                    m_breakIntoDebugger = false;
                } else {
                    validCommand = false;
                }

            } else if (tokens[0] == "breakpoint" || tokens[0] == "b") {
                validCommand = false;
                if (tokens.size() > 1 && tokens[1][0] == '$') {
                    uint16_t address = HexStringToIntegral<uint16_t>(tokens[1].substr(1).c_str());
                    if (auto bp = breakpoints.Add(address)) {
                        std::cout << FormattedString<>("Added breakpoint at $%04x", address)
                                  << std::endl;
                        validCommand = true;
                    }
                }

            } else if (tokens[0] == "delete") {
                validCommand = false;
                if (tokens.size() > 1) {
                    int breakpointIndex = std::stoi(tokens[1]);
                    if (auto bp = breakpoints.RemoveAtIndex(breakpointIndex)) {
                        std::cout << FormattedString<>("Deleted breakpoint %d at $%04x",
                                                       breakpointIndex, bp->address)
                                  << std::endl;
                        validCommand = true;
                    } else {
                        std::cout << "Invalid breakpoint specified" << std::endl;
                    }
                }

            } else if (tokens[0] == "enable") {
                validCommand = false;
                if (tokens.size() > 1) {
                    size_t breakpointIndex = std::stoi(tokens[1]);
                    if (auto bp = breakpoints.GetAtIndex(breakpointIndex)) {
                        bp->enabled = true;
                        std::cout << FormattedString<>("Enabled breakpoint %d at $%04x",
                                                       breakpointIndex, bp->address)
                                  << std::endl;
                        validCommand = true;
                    } else {
                        std::cout << "Invalid breakpoint specified" << std::endl;
                    }
                }

            } else if (tokens[0] == "disable") {
                validCommand = false;
                if (tokens.size() > 1) {
                    size_t breakpointIndex = std::stoi(tokens[1]);
                    if (auto bp = breakpoints.GetAtIndex(breakpointIndex)) {
                        bp->enabled = false;
                        std::cout << FormattedString<>("Disabled breakpoint %d at $%04x",
                                                       breakpointIndex, bp->address)
                                  << std::endl;
                        validCommand = true;
                    } else {
                        std::cout << "Invalid breakpoint specified" << std::endl;
                    }
                }

            } else if (tokens[0] == "info") {
                if (tokens.size() > 1 && (tokens[1] == "registers" || tokens[1] == "reg")) {
                    PrintRegisters(m_cpu->Registers());
                } else if (tokens.size() > 1 && (tokens[1] == "break")) {
                    std::cout << "Breakpoints:\n";
                    for (size_t i = 0; i < breakpoints.Num(); ++i) {
                        auto bp = breakpoints.GetAtIndex(i);
                        std::cout << FormattedString<>("%3d: $%04x\t%s\n", i, bp->address,
                                                       bp->enabled ? "Enabled" : "Disabled");
                    }
                    std::cout << std::flush;

                } else {
                    validCommand = false;
                }

            } else if (tokens[0] == "print" || tokens[0] == "p") {
                if (tokens.size() > 1 && tokens[1][0] == '$') {
                    uint16_t address = HexStringToIntegral<uint16_t>(tokens[1].substr(1).c_str());
                    uint8_t value = m_memoryBus->Read(address);
                    std::cout << FormattedString<>("$%04x = $%02x (%d)", address, value, value)
                              << std::endl;
                } else {
                    validCommand = false;
                }

            } else if (tokens[0] == "loadsymbols") {
                if (tokens.size() > 1 && LoadUserSymbolsFile(tokens[1].c_str(), m_symbolTable)) {
                    std::cout << FormattedString<>("Loaded symbols from %s", tokens[1].c_str())
                              << std::endl;
                } else {
                    validCommand = false;
                }

            } else {
                validCommand = false;
            }

            if (validCommand) {
                m_lastCommand = input;
            } else {
                std::cout << "Invalid command: " << input << std::endl;
            }

        } else { // Not broken into debugger (running)

            if (m_traceEnabled)
                PrintOp(m_cpu->Registers(), *m_memoryBus, m_symbolTable);

            m_cpu->ExecuteInstruction();

            if (auto bp = breakpoints.Get(m_cpu->Registers().PC)) {
                if (bp->autoDelete) {
                    breakpoints.Remove(m_cpu->Registers().PC);
                    m_breakIntoDebugger = true;
                } else if (bp->enabled) {
                    std::cout << FormattedString<>("Breakpoint hit at %04x", bp->address)
                              << std::endl;
                    m_breakIntoDebugger = true;
                }
            }
        }
    }
}
