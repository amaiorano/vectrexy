#pragma once

#include "Base.h"
#include "ConsoleOutput.h"

namespace ErrorHandler {
    enum class Policy { Ignore, Log, Fail };

    inline Policy g_policy = Policy::Ignore;

    template <typename... Args>
    void Undefined(const char* format, Args... args) {
        switch (g_policy) {
        case Policy::Ignore:
            break;

        case Policy::Log:
            Errorf(format, args...);
            break;

        case Policy::Fail:
            FAIL_MSG(format, args...);
            break;
        }
    }
} // namespace ErrorHandler
