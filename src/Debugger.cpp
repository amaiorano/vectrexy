#include "Debugger.h"
#include "Cpu.h"
#include "MemoryBus.h"
#include "Platform.h"
#include <iostream>
#include <string>

void Debugger::Init(MemoryBus& memoryBus, Cpu& cpu) {
    m_memoryBus = &memoryBus;
    m_cpu = &cpu;

    Platform::SetConsoleCtrlHandler([this] {
        m_breakIntoDebugger = true;
        return true;
    });
}

void Debugger::Run() {
    while (true) {
        if (m_breakIntoDebugger) {
            auto& reg = m_cpu->Registers();
            std::cout << FormattedString<>("[$%x]>", reg.PC);
            std::string input;
            std::getline(std::cin, input); //@TODO: check return value

            if (input.find("c") != -1 || input.find("continue") != -1)
                m_breakIntoDebugger = false;
            else
                m_cpu->ExecuteInstruction();
        } else {
            m_cpu->ExecuteInstruction();
        }
    }
}
