#include "Debugger.h"
#include "Cpu.h"
#include "CpuHelpers.h"
#include "CpuOpCodes.h"
#include "MemoryBus.h"
#include "Platform.h"
#include <array>
#include <iostream>
#include <vector>

namespace {
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
            //@TODO: Some inherent instructions do take operands, like TFR
            disasmInstruction = cpuOp.name;
        } break;

        case AddressingMode::Immediate: {
            uint8_t value = instruction.operands[0];
            disasmInstruction = FormattedString<>("%s #$%02x", cpuOp.name, value);
            comment = FormattedString<>("(%d)", value);
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

        case AddressingMode::Indexed:  //@TODO
        case AddressingMode::Relative: //@TODO
            disasmInstruction = cpuOp.name;
            break;

        case AddressingMode::Illegal:
        case AddressingMode::Variant:
            assert(false);
            break;
        }

        std::string result =
            FormattedString<>("%-10s %-10s %s", hexInstruction.c_str(), disasmInstruction.c_str(),
                              comment.size() > 0 ? ("# " + comment).c_str() : "");
        return result;
    };

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

    auto PrintOp = [&] {
        auto& reg = m_cpu->Registers();
        std::string op = DisassembleOp(reg, *m_memoryBus);
        std::cout << FormattedString<>("[$%x] %s\n", reg.PC, op.c_str());
    };

    while (true) {
        if (m_breakIntoDebugger) {
            PrintOp();
            std::cout << FormattedString<>("(%s)>", m_lastCommand.c_str());

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
            } else if (tokens[0] == "continue" || tokens[0] == "c") {
                m_breakIntoDebugger = false;
            } else if (tokens[0] == "step" || tokens[0] == "s") {
                // "Step into"
                m_cpu->ExecuteInstruction();
            } else {
                std::cout << "Invalid command: " << input << std::endl;
                validCommand = false;
            }

            if (validCommand)
                m_lastCommand = input;

        } else {
            if (m_traceEnabled)
                PrintOp();

            m_cpu->ExecuteInstruction();
        }
    }
}
