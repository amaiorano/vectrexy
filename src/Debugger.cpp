#include "Debugger.h"
#include "Cpu.h"
#include "CpuHelpers.h"
#include "CpuOpCodes.h"
#include "MemoryBus.h"
#include "Platform.h"
#include <array>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
    template <typename T>
    T HexStringToIntegral(const char* s) {
        std::stringstream converter(s);
        T value;
        converter >> std::hex >> value;
        return value;
    }

    std::vector<std::string> Tokenize(const std::string& s) {
        std::vector<std::string> result;
        const char* whitespace = " \t";
        size_t startIndex = 0;
        while ((startIndex = s.find_first_not_of(whitespace, startIndex)) != std::string::npos) {
            size_t endIndex = s.find_first_of(whitespace, startIndex + 1);

            if (endIndex == std::string::npos) {
                result.emplace_back(s.substr(startIndex));
                break;
            } else {
                result.emplace_back(s.substr(startIndex, endIndex - startIndex));
                startIndex = endIndex;
            }
        }
        return result;
    }

    struct Instruction {
        const CpuOp& cpuOp;
        int page;
        std::array<uint8_t, 2> operands;
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
        const int numOperands = cpuOp.size - 1 - (cpuOpPage == 0 ? 0 : 1);

        Instruction result = {cpuOp, cpuOpPage};
        for (int i = 0; i < numOperands; ++i) {
            result.operands[i] = memoryBus.Read(opAddr++);
        }
        return result;
    }

    std::string DisassembleOp(const CpuRegisters& reg, const MemoryBus& memoryBus) {
        uint16_t opAddr = reg.PC;
        auto instruction = ReadInstruction(opAddr, memoryBus);
        const auto& cpuOp = instruction.cpuOp;

        std::string hexInstruction;
        // Output instruction in hex
        for (uint16_t i = 0; i < cpuOp.size; ++i)
            hexInstruction += FormattedString<>("%02x", memoryBus.Read(opAddr + i));

        std::string disasmInstruction, comment;

        switch (cpuOp.addrMode) {
        case AddressingMode::Inherent: {
            // Handle specific instructions that take operands (most inherent instructions don't)
            if (cpuOp.opCode == 0x1E /*EXG*/ || cpuOp.opCode == 0x1F /*TFR*/) {
                uint8_t postbyte = instruction.operands[0];
                uint8_t src = (postbyte >> 4) & 0b111;
                uint8_t dst = postbyte & 0b111;
                if (postbyte & BITS(3)) {
                    char* const regName[]{"A", "B", "CC", "DP"};
                    disasmInstruction =
                        FormattedString<>("%s %s,%s", cpuOp.name, regName[src], regName[dst]);
                } else {
                    char* const regName[]{"D", "X", "Y", "U", "S", "PC"};
                    disasmInstruction =
                        FormattedString<>("%s %s,%s", cpuOp.name, regName[src], regName[dst]);
                }
            } else {
                disasmInstruction = cpuOp.name;
            }
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
            uint16_t EA = CombineToU16(reg.DP, instruction.operands[0]);
            uint8_t value = memoryBus.Read(EA);
            disasmInstruction = FormattedString<>("%s $%02x", cpuOp.name, instruction.operands[0]);
            comment = FormattedString<>("DP:(PC) = $%02x = $%02x (%d)", EA, value, value);
        } break;

        case AddressingMode::Indexed: { //@TODO
            disasmInstruction = cpuOp.name;
        } break;

        case AddressingMode::Relative: {
            // Branch instruction with 8 or 16 bit signed relative offset
            auto nextPC = reg.PC + cpuOp.size;
            if (cpuOp.size == 2) {
                auto offset = static_cast<int8_t>(instruction.operands[0]);
                disasmInstruction = FormattedString<>("%s $%02x", cpuOp.name, U16(offset) & 0x00FF);
                comment = FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
            } else {
                assert(cpuOp.size == 3);
                auto offset = static_cast<int16_t>(
                    CombineToU16(instruction.operands[0], instruction.operands[1]));
                disasmInstruction = FormattedString<>("%s $%04x", cpuOp.name, offset);
                comment = FormattedString<>("(%d), PC + offset = $%04x", offset, nextPC + offset);
            }
        } break;

        case AddressingMode::Illegal: {
        case AddressingMode::Variant:
            assert(false);
        } break;
        }

        std::string result =
            FormattedString<>("%-10s %-10s %s", hexInstruction.c_str(), disasmInstruction.c_str(),
                              comment.size() > 0 ? ("# " + comment).c_str() : "");
        return result;
    };

    void PrintOp(const CpuRegisters& reg, const MemoryBus& memoryBus) {
        std::string op = DisassembleOp(reg, memoryBus);
        std::cout << FormattedString<>("[$%x] %s\n", reg.PC, op.c_str());
    }

    void PrintRegisters(const CpuRegisters& reg) {
        const auto& cc = reg.CC;

        std::string CC = FormattedString<>(
            "%c%c%c%c%c%c%c%c", cc.Carry ? 'C' : 'c', cc.Overflow ? 'V' : 'v', cc.Zero ? 'Z' : 'z',
            cc.Negative ? 'N' : 'n', cc.InterruptMask ? 'I' : 'i', cc.HalfCarry ? 'H' : 'h',
            cc.FastInterruptMask ? 'F' : 'f', cc.Entire ? 'E' : 'e');

        std::cout << FormattedString<>("A=$%02x (%d) B=$%02x (%d) D=$%04x (%d) X=$%04x (%d) "
                                       "Y=$%04x (%d) U=$%04x S=$%04x DP=$%02x PC=$%04x CC=%s\n",
                                       reg.A, reg.A, reg.B, reg.B, reg.D, reg.D, reg.X, reg.X,
                                       reg.Y, reg.Y, reg.U, reg.S, reg.DP, reg.PC, CC.c_str());
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

    while (true) {
        if (m_breakIntoDebugger) {
            std::cout << FormattedString<>("$%04x (%s)>", m_cpu->Registers().PC,
                                           m_lastCommand.c_str());

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

            } else if (tokens[0] == "continue" || tokens[0] == "c") {
                m_breakIntoDebugger = false;

            } else if (tokens[0] == "step" || tokens[0] == "s") {
                // "Step into"
                PrintOp(m_cpu->Registers(), *m_memoryBus);
                m_cpu->ExecuteInstruction();

            } else if (tokens[0] == "info") {
                if (tokens.size() > 1 && (tokens[1] == "registers" || tokens[1] == "reg")) {
                    PrintRegisters(m_cpu->Registers());
                } else {
                    validCommand = false;
                }

            } else if (tokens[0] == "print" || tokens[0] == "p") {
                if (tokens.size() > 1 && tokens[1][0] == '$') {
                    uint16_t address = HexStringToIntegral<uint16_t>(tokens[1].substr(1).c_str());
                    uint8_t value = m_memoryBus->Read(address);
                    std::cout << FormattedString<>("$%04x = $%02x (%d)\n", address, value, value);
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

        } else {
            if (m_traceEnabled)
                PrintOp(m_cpu->Registers(), *m_memoryBus);

            m_cpu->ExecuteInstruction();
        }
    }
}
