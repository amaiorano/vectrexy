#pragma once

#include "core/Base.h"
#include "core/ConsoleOutput.h"

namespace ErrorHandler {
    enum class Policy {
        Ignore,  // Ignore error
        Log,     // Log error (even if repeated)
        LogOnce, // Log error only once (ignore repeated instances of the error)
        Fail     // Fail hard
    };

    constexpr Policy DefaultPolicy = Policy::LogOnce;

    void SetPolicy(Policy policy);
    void Reset();

    namespace Internal {
        void DoHandleError(const char* messagePrefix, const char* message);
    } // namespace Internal

    template <typename... Args>
    void Undefined(const char* format, Args... args) {
        Internal::DoHandleError("[Undefined] ",
                                FormattedString<>(format, std::forward<Args>(args)...));
    }

    template <typename... Args>
    void Unsupported(const char* format, Args... args) {
        Internal::DoHandleError("[Unsupported] ",
                                FormattedString<>(format, std::forward<Args>(args)...));
    }

} // namespace ErrorHandler
