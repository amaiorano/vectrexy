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

namespace Platform {

    void SetConsoleCtrlHandler(std::function<bool()> handler) {
        g_consoleCtrlHandler = std::move(handler);
        BOOL succeeded =
            ::SetConsoleCtrlHandler(&Win32ConsoleCtrlHandler, g_consoleCtrlHandler != nullptr);
        (void)succeeded;
        assert(succeeded);
    }

    void SetConsoleColor(ConsoleColor foreground, ConsoleColor background) {
        HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
        WORD colorAttribute = static_cast<WORD>(foreground) + static_cast<WORD>(background) * 16;
        ::SetConsoleTextAttribute(hConsole, colorAttribute);
    }

    std::tuple<ConsoleColor, ConsoleColor> GetConsoleColor() {
        HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO info;
        ::GetConsoleScreenBufferInfo(hConsole, &info);
        WORD foreground = info.wAttributes & 0b1111;
        WORD background = (info.wAttributes & 0b1111'0000) >> 4;
        return {static_cast<ConsoleColor>(foreground), static_cast<ConsoleColor>(background)};
    }

} // namespace Platform
#endif
