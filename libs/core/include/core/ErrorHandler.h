#pragma once

#include "core/Base.h"
#include "core/ConsoleOutput.h"

namespace ErrorHandler {
    enum class Policy { Ignore, Log, Fail };

    inline Policy g_policy = Policy::Ignore;

    namespace Internal {
        template <typename... Args>
        void HandleError(const char* errorType, const char* format, Args... args) {
            switch (g_policy) {
            case Policy::Ignore:
                break;

            case Policy::Log:
                Errorf(FormattedString<>("%s %s", errorType, format), std::forward<Args>(args)...);
                break;

            case Policy::Fail:
                FAIL_MSG(FormattedString<>("%s %s", errorType, format),
                         std::forward<Args>(args)...);
                break;
            }
        }
    } // namespace Internal

    template <typename... Args>
    void Undefined(const char* format, Args... args) {
        Internal::HandleError("[Undefined]", format, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void Unsupported(const char* format, Args... args) {
        Internal::HandleError("[Unsupported]", format, std::forward<Args>(args)...);
    }

} // namespace ErrorHandler
