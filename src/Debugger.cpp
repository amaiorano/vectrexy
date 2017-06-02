#include "Debugger.h"
#include "Cpu.h"
#include "MemoryBus.h"
#include "Platform.h"
#include <iostream>
#include <vector>

void Debugger::Init(MemoryBus& memoryBus, Cpu& cpu) {
    m_memoryBus = &memoryBus;
    m_cpu = &cpu;

    Platform::SetConsoleCtrlHandler([this] {
        m_breakIntoDebugger = true;
        return true;
    });
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

void Debugger::Run() {
    m_lastCommand = "step"; // Reasonable default

    while (true) {
        if (m_breakIntoDebugger) {
            auto& reg = m_cpu->Registers();
            std::cout << FormattedString<>("[$%x] (%s)>", reg.PC, m_lastCommand.c_str());

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
            m_cpu->ExecuteInstruction();
        }
    }
}
