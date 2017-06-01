#pragma once

#include <functional>

namespace Platform {
    // Set handler for Ctrl+C. Handler must return true if handled, or false to allow default system
    // handler. Pass in nullptr to unset handler.
    void SetConsoleCtrlHandler(std::function<bool()> handler);

} // namespace Platform
