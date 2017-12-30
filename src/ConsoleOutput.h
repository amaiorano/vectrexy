#pragma once

#include <cassert>
#include <cstdio>

namespace internal {
    inline FILE* g_printStream = stdout;
    inline FILE* g_errorStream = stderr;
} // namespace internal

enum class ConsoleStream { Output, Error };

constexpr FILE* GetStream(ConsoleStream type) {
    switch (type) {
    case ConsoleStream::Output:
        return internal::g_printStream;
    case ConsoleStream::Error:
        return internal::g_errorStream;
    }
    return nullptr;
}

template <typename... Args>
void Consolef(ConsoleStream type, const char* format, Args... args) {
    fprintf(GetStream(type), format, args...);
}

template <typename... Args>
void Printf(const char* format, Args... args) {
    fprintf(GetStream(ConsoleStream::Output), format, args...);
}

template <typename... Args>
void Errorf(const char* format, Args... args) {
    fprintf(GetStream(ConsoleStream::Error), format, args...);
}

inline void FlushStream(ConsoleStream type) {
    fflush(GetStream(type));
}

// Use to override current print stream
class ScopedOverridePrintStream {
public:
    ScopedOverridePrintStream() = default;
    ScopedOverridePrintStream(FILE* stream) { SetPrintStream(stream); }
    ~ScopedOverridePrintStream() {
        if (m_oldStream)
            internal::g_printStream = m_oldStream;
    }

    void SetPrintStream(FILE* stream) {
        assert(m_oldStream == nullptr);
        m_oldStream = internal::g_printStream;
        internal::g_printStream = stream;
    }

private:
    FILE* m_oldStream = nullptr;
};
