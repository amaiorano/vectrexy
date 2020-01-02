#include "emulator/DevMemoryDevice.h"
#include "core/ConsoleOutput.h"
#include "core/ErrorHandler.h"
#include "emulator/MemoryMap.h"
#include <cinttypes>
#include <cstdarg>

namespace {
    // New memory-mapped registers

    // Printf-registers: first write each arg in left-to-right order by writing to one of the PUSH
    // registers, then write the format string to DEV_PRINTF_FORMAT (2 bytes), upon which the string
    // will be formatted and printed to stdout.
    constexpr uint16_t DEV_PRINTF_PUSH_ARG8 = 0xC100;
    constexpr uint16_t DEV_PRINTF_PUSH_ARG16[2] = {0xC101, 0xC102};
    constexpr uint16_t DEV_PRINTF_PUSH_CSTR[2] = {0xC103, 0xC104};
    constexpr uint16_t DEV_PRINTF_FORMAT[2] = {0xC105, 0xC106};
} // namespace

void DevMemoryDevice::Init(MemoryBus& memoryBus) {
    m_memoryBus = &memoryBus;
    memoryBus.ConnectDevice(*this, MemoryMap::Unmapped.range, EnableSync::False);
}

uint8_t DevMemoryDevice::Read(uint16_t address) const {
    ErrorHandler::Undefined("Read from unmapped range at address $%04x\n", address);
    return 0;
}

void DevMemoryDevice::Write(uint16_t address, uint8_t value) {
    if (HandleDevWrite(address, value))
        return;

    ErrorHandler::Undefined("Write to unmappped range of value $%02x at address $%04x\n", value,
                            address);
}

bool DevMemoryDevice::HandleDevWrite(uint16_t address, uint8_t value) {
    auto readString = [&](uint16_t stringAddress) -> std::string {
        char c = 0;
        std::string s;
        do {
            c = m_memoryBus->ReadRaw(stringAddress++);
            s += c;
        } while (c != 0);
        return s;
    };

    switch (address) {
    case DEV_PRINTF_PUSH_ARG8: {
        // NOTE: Looks like integrals are stored as words for va_list (on Win64?)
        size_t v = value;
        size_t* pv = (size_t*)(&m_printfData.args[m_printfData.argIndex]);
        *pv = v;
        m_printfData.argIndex += sizeof(v);
        return true;
    }

    case DEV_PRINTF_PUSH_ARG16[0]:
        m_opFirstByte = value;
        return true;

    case DEV_PRINTF_PUSH_ARG16[1]: {
        size_t v = static_cast<uint16_t>(m_opFirstByte) << 8 | static_cast<uint16_t>(value);
        size_t* pv = (size_t*)(&m_printfData.args[m_printfData.argIndex]);
        *pv = v;
        m_printfData.argIndex += sizeof(v);
        return true;
    }

    case DEV_PRINTF_PUSH_CSTR[0]:
        m_opFirstByte = value;
        return true;

    case DEV_PRINTF_PUSH_CSTR[1]: {
        const uint16_t stringAddress =
            static_cast<uint16_t>(m_opFirstByte << 8) | static_cast<uint16_t>(value);
        auto s = readString(stringAddress);
        m_printfData.strings.emplace_back(move(s));

        // Write the address of the copied string to args
        const char** p = (const char**)&m_printfData.args[m_printfData.argIndex];
        *p = m_printfData.strings.back().data();
        m_printfData.argIndex += sizeof(p);
        return true;
    }

    case DEV_PRINTF_FORMAT[0]: {
        m_opFirstByte = value;
        return true;
    }

    case DEV_PRINTF_FORMAT[1]: {
        const uint16_t formatStringAddress =
            static_cast<uint16_t>(m_opFirstByte << 8) | static_cast<uint16_t>(value);

        auto format = readString(formatStringAddress);
        auto data = m_printfData.args.data();
        char text[2048];
        vsprintf(text, format.c_str(), *reinterpret_cast<va_list*>(&data));
        
        Printf("[DEV] %s", text);
        FlushStream(ConsoleStream::Output);

        // Reset for next printf call
        m_printfData.Reset();

        return true;
    }
    }

    return false;
}
