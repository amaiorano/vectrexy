#include "Platform.h"

#ifdef WIN32

#include <cassert>
#include <windows.h>

namespace {
    std::function<bool()> g_consoleCtrlHandler;
    BOOL WINAPI Win32ConsoleCtrlHandler(DWORD /*dwCtrlType*/) {
        assert(g_consoleCtrlHandler);
        return !!g_consoleCtrlHandler();
    }

} // namespace

void Platform::SetConsoleCtrlHandler(std::function<bool()> handler) {
    g_consoleCtrlHandler = std::move(handler);
    BOOL succeeded =
        ::SetConsoleCtrlHandler(&Win32ConsoleCtrlHandler, g_consoleCtrlHandler != nullptr);
    assert(succeeded);
}

#endif
