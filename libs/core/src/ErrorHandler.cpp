#include "core/ErrorHandler.h"
#include <unordered_set>

namespace {
    ErrorHandler::Policy g_policy = ErrorHandler::DefaultPolicy;
    std::unordered_set<std::string> g_errorMessages;
} // namespace

namespace ErrorHandler {
    void SetPolicy(Policy policy) { g_policy = policy; }
    void Reset() { g_errorMessages.clear(); }

    void Internal::DoHandleError(const char* messagePrefix, const char* message) {
        switch (g_policy) {
        case Policy::Ignore:
            break;

        case Policy::Log:
            Errorf("%s%s", messagePrefix, message);
            break;

        case Policy::LogOnce: {
            auto pair = g_errorMessages.insert(message);
            // pair.second denotes if insertion took place (only true the first time)
            if (pair.second) {
                Errorf("%s%s", messagePrefix, message);
            }
        } break;

        case Policy::Fail:
            FAIL_MSG("%s%s", messagePrefix, message);
            break;
        }
    }
} // namespace ErrorHandler
