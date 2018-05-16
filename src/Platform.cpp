#include "Platform.h"
#include "Base.h"

#if defined(PLATFORM_WINDOWS)

#include <cassert>
struct IUnknown; // Fix compile error in VS2017 15.3 when including windows.h
#include <windows.h>

// Undef the macro from windows.h so we can use the same name
#undef SetConsoleTitle

namespace {
    std::function<bool()> g_consoleCtrlHandler;
    BOOL WINAPI Win32ConsoleCtrlHandler(DWORD /*dwCtrlType*/) {
        assert(g_consoleCtrlHandler);
        return !!g_consoleCtrlHandler();
    }

} // namespace

namespace Platform {
    void SetFocus(WindowHandle windowHandle) {
        // Global mutex to avoid deadlock when multiple instances attempt to attach thread input
        auto mutex = ::CreateMutex(NULL, FALSE, "Global\\VectrexySetFocusMutex");
        ::WaitForSingleObject(mutex, INFINITE);

        HWND hwnd = std::any_cast<HWND>(windowHandle);
        auto foreThread = GetWindowThreadProcessId(GetForegroundWindow(), 0);
        auto appThread = GetCurrentThreadId();
        if (foreThread != appThread) {
            if (AttachThreadInput(foreThread, appThread, true))
                BringWindowToTop(hwnd);
            AttachThreadInput(foreThread, appThread, false);
        } else {
            BringWindowToTop(hwnd);
        }

        ::ReleaseMutex(mutex);
    }

    void SetConsoleFocus() { Platform::SetFocus(::GetConsoleWindow()); }

    void SetConsoleTitle(const char* title) { ::SetConsoleTitleA(title); }

    void SetConsoleCtrlHandler(std::function<bool()> handler) {
        g_consoleCtrlHandler = std::move(handler);
        BOOL succeeded =
            ::SetConsoleCtrlHandler(&Win32ConsoleCtrlHandler, g_consoleCtrlHandler != nullptr);
        (void)succeeded;
        assert(succeeded);
    }

    std::function<bool()> GetConsoleCtrlHandler() { return g_consoleCtrlHandler; }

    static bool g_consoleColorEnabled = true;

    void SetConsoleColoringEnabled(bool enabled) {
        if (!enabled) {
            SetConsoleColor(ConsoleColor::White, ConsoleColor::Black);
        }
        g_consoleColorEnabled = enabled;
    }

    bool IsConsoleColoringEnabled() { return g_consoleColorEnabled; }

    void SetConsoleColor(ConsoleColor foreground, ConsoleColor background) {
        if (!g_consoleColorEnabled)
            return;
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

    bool SupportsOpenFileDialog() { return true; }

    std::optional<std::string> OpenFileDialog(const char* title, const char* filterName,
                                              const char* filterTypes,
                                              std::optional<fs::path> initialDirectory) {
        auto filter =
            FormattedString<>("%s (%s)%c%s%c", filterName, filterTypes, '\0', filterTypes, '\0');

        char file[_MAX_PATH] = "";
        char currDir[_MAX_PATH] = "";
        ::GetCurrentDirectoryA(sizeof(currDir), currDir);

        // Note: If lpstrInitialDir has the same value as was passed the first time the application
        // used an Open or Save As dialog box, the path most recently selected by the user is used
        // as the initial directory.
        std::string initDir = currDir;
        if (initialDirectory.has_value() && fs::exists(*initialDirectory)) {
            initDir = initialDirectory->string();
        }

        OPENFILENAMEA ofn = {0};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFile = file;
        ofn.nMaxFile = sizeof(file);
        ofn.lpstrTitle = title;
        ofn.lpstrFilter = filter.Value();
        ofn.nFilterIndex = 0;
        ofn.lpstrInitialDir = initDir.c_str();
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (::GetOpenFileNameA(&ofn) == TRUE) {
            return {ofn.lpstrFile};
        }
        return {};
    }

    void ExecuteShellCommand(const char* command) {
        ::ShellExecute(NULL, "open", command, NULL, NULL, SW_SHOWNORMAL);
    }
} // namespace Platform

#elif defined(PLATFORM_LINUX)

namespace {
    std::function<bool()> g_consoleCtrlHandler;
    bool g_consoleColorEnabled = true;
} // namespace

namespace Platform {
    void SetFocus(WindowHandle windowHandle) {}

    void SetConsoleFocus() {}

    void SetConsoleTitle(const char* title) {}

    void SetConsoleCtrlHandler(std::function<bool()> handler) {
        g_consoleCtrlHandler = std::move(handler);
    }

    std::function<bool()> GetConsoleCtrlHandler() { return g_consoleCtrlHandler; }

    void SetConsoleColoringEnabled(bool enabled) {
        if (!enabled) {
            SetConsoleColor(ConsoleColor::White, ConsoleColor::Black);
        }
        g_consoleColorEnabled = enabled;
    }

    bool IsConsoleColoringEnabled() { return g_consoleColorEnabled; }

    void SetConsoleColor(ConsoleColor foreground, ConsoleColor background) {
        if (!g_consoleColorEnabled)
            return;
    }

    std::tuple<ConsoleColor, ConsoleColor> GetConsoleColor() {
        return {ConsoleColor::Black, ConsoleColor::White};
    }

    bool SupportsOpenFileDialog() { return false; }

    std::optional<std::string> OpenFileDialog(const char* title, const char* filterName,
                                              const char* filterTypes,
                                              std::optional<fs::path> initialDirectory) {
        return {};
    }

    void ExecuteShellCommand(const char* command) {}
} // namespace Platform

#endif
