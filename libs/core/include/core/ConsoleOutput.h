#pragma once

#include <cassert>
#include <cstdio>

namespace internal {
    inline FILE* g_printStream = stdout;
    inline FILE* g_errorStream = stderr;
} // namespace internal

enum class ConsoleStream { Output, Error };

inline FILE* GetStream(ConsoleStream type) {
    switch (type) {
    case ConsoleStream::Output:
        return internal::g_printStream;
    case ConsoleStream::Error:
        return internal::g_errorStream;
    }
    return nullptr;
}

inline void SetStream(ConsoleStream type, FILE* stream) {
    switch (type) {
    case ConsoleStream::Output:
        internal::g_printStream = stream;
    case ConsoleStream::Error:
        internal::g_errorStream = stream;
    }
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

// After calling Rewind, the next print will overwrite the current line
inline void Rewind(ConsoleStream type) {
    Consolef(type, "\033[A\033[2K");
    rewind(GetStream(type));
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
