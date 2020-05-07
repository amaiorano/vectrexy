#include "core/Platform.h"
#include "core/Base.h"
#include "core/FileSystem.h"
#include "core/FileSystemUtil.h"

#include <iostream>
#include <string>

#if defined(PLATFORM_WINDOWS)

#include <cassert>
struct IUnknown; // Fix compile error in VS2017 15.3 when including windows.h
#include <windows.h>

namespace {
    std::function<bool()> g_consoleCtrlHandler;
    BOOL WINAPI Win32ConsoleCtrlHandler(DWORD /*dwCtrlType*/) {
        assert(g_consoleCtrlHandler);
        return !!g_consoleCtrlHandler();
    }

} // namespace

namespace Platform {
    void InitConsole() {
        // Enable VT100 style in current console. This allows us to use codes like "\033[A\033[2K"
        // to delete current line and bring cursor back to column 0.
        HANDLE hConsole = ::GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode;
        ::GetConsoleMode(hConsole, &mode);
        ::SetConsoleMode(hConsole,
                         mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
    }

    void SetFocus(WindowHandle windowHandle) {
        // Global mutex to avoid deadlock when multiple instances attempt to attach thread input
        auto mutex = ::CreateMutex(nullptr, FALSE, "Global\\VectrexySetFocusMutex");
        ::WaitForSingleObject(mutex, INFINITE);

        HWND hwnd = std::any_cast<HWND>(windowHandle);
        auto foreThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
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

    std::string ConsoleReadLine(const char* prompt) {
        printf(prompt);
        std::string result;
        while (true) {
            fflush(stdout);
            if (const auto& stream = std::getline(std::cin, result))
                return result;

            // getline will fail under certain conditions, like when Ctrl+C is pressed, in
            // which case we just clear the stream status and restart the loop.
            std::cin.clear();
        }
        return {};
    }

    bool ExecuteShellCommand(const char* command) {
        ::ShellExecute(nullptr, "open", command, nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }

    void WaitForDebuggerAttach(bool breakOnAttach) {
        if (::IsDebuggerPresent())
            return;

        int msgboxID = ::MessageBox(nullptr, "Wait for debugger to attach?", "Attach Debugger",
                                    MB_ICONEXCLAMATION | MB_YESNO);

        if (msgboxID == IDNO) {
            return;
        }

        while (!::IsDebuggerPresent())
            ::Sleep(100);

        if (breakOnAttach)
            ::DebugBreak();
    }

} // namespace Platform

#elif defined(PLATFORM_LINUX)

#include "linenoise/linenoise.h"
#include <signal.h>
#include <unistd.h>

namespace {
    std::function<bool()> g_consoleCtrlHandler;
    void ConsoleCtrlHandler(int /*signum*/) {
        assert(g_consoleCtrlHandler);
        bool result = g_consoleCtrlHandler();
        (void)result; // On Linux, result isn't returned
    }

    bool g_consoleColorEnabled = true;
} // namespace

namespace Platform {
    void InitConsole() {}

    void SetFocus(WindowHandle windowHandle) {}

    void SetConsoleFocus() {}

    void SetConsoleCtrlHandler(std::function<bool()> handler) {
        g_consoleCtrlHandler = std::move(handler);

        struct sigaction sa;
        sa.sa_handler = g_consoleCtrlHandler ? ConsoleCtrlHandler : SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART; // Restart functions if interrupted by handler
        auto result = sigaction(SIGINT, &sa, NULL);
        assert(result == 0);
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

    std::string ConsoleReadLine(const char* prompt) {
        while (true) {
            fflush(stdout);
            if (char* line = linenoise(prompt)) {
                std::string result = line;
                linenoiseHistoryAdd(line);
                free(line);
                return result;
            }
            // linenoise returns nullptr under certain conditions, like if Ctrl+C is pressed, so we
            // just keep looping until we get a valid string.
        }
        return {};
    }

    bool ExecuteShellCommand(const char* command) {
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            if (execlp("xdg-open", "xdg-open", command, NULL) < 0)
                return false;
        } else if (pid == -1) {
            return false;
        }
        // Parent process
        return true;
    }

    void WaitForDebuggerAttach(bool breakOnAttach) { (void)breakOnAttach; }
} // namespace Platform

#endif

// Implement Platform::OpenFileDialog using no_file_dialog
#if defined(PLATFORM_WINDOWS)
#define NOC_FILE_DIALOG_IMPLEMENTATION
#define NOC_FILE_DIALOG_WIN32
MSC_PUSH_WARNING_DISABLE(4996  // 'strdup': The POSIX name for this item is deprecated.
                         4100) // unreferenced formal parameter
#include "noc/noc_file_dialog.h "
MSC_POP_WARNING_DISABLE()
#elif defined(PLATFORM_LINUX)
#define NOC_FILE_DIALOG_IMPLEMENTATION
#define NOC_FILE_DIALOG_GTK
#include "noc/noc_file_dialog.h"
#else
#error Implement me for current platform
#endif

namespace Platform {
    std::optional<std::string> OpenFileDialog(const char* /*title*/, const char* filterName,
                                              const char* filterTypes,
                                              std::optional<fs::path> initialPath) {

        auto filter =
            FormattedString<>("%s (%s)%c%s%c", filterName, filterTypes, '\0', filterTypes, '\0');

        if (!(initialPath.has_value() && fs::exists(*initialPath))) {
            initialPath = fs::current_path();
        }

        // Make sure to restore original current directory after opening dialog
        FileSystemUtil::ScopedSetCurrentDirectory scopedSetDir;

        auto result = noc_file_dialog_open(NOC_FILE_DIALOG_OPEN, filter,
                                           initialPath->make_preferred().string().c_str(), nullptr);
        if (result)
            return result;
        return {};
    }
} // namespace Platform
