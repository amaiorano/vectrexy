#pragma once

#include "core/FileSystem.h"
#include <any>
#include <functional>
#include <optional>
#include <tuple>

namespace Platform {
    // Every platform has their own window handle type, so we use std::any and cast internally
    using WindowHandle = std::any;

    void InitConsole();

    void SetFocus(WindowHandle windowHandle);

    void SetConsoleFocus();

    // Set handler for Ctrl+C. Handler must return true if handled, or false to allow default system
    // handler. Pass in nullptr to unset handler.
    void SetConsoleCtrlHandler(std::function<bool()> handler);
    std::function<bool()> GetConsoleCtrlHandler();

    void SetConsoleColoringEnabled(bool enabled);
    bool IsConsoleColoringEnabled();

    enum class ConsoleColor {
        Black,
        Blue,
        Green,
        Aqua,
        Red,
        Purple,
        Yellow,
        White,
        Gray,
        LightBlue,
        LightGreen,
        LightAqua,
        LightRed,
        LightPurple,
        LightYellow,
        BrightWhite,
    };

    void SetConsoleColor(ConsoleColor foreground, ConsoleColor background = ConsoleColor::Black);
    std::tuple<ConsoleColor, ConsoleColor> GetConsoleColor();

    struct ScopedConsoleColor {
        ScopedConsoleColor() { m_color = GetConsoleColor(); }
        ScopedConsoleColor(ConsoleColor foreground, ConsoleColor background = ConsoleColor::Black) {
            m_color = GetConsoleColor();
            SetConsoleColor(foreground, background);
        }
        ~ScopedConsoleColor() { SetConsoleColor(std::get<0>(m_color), std::get<1>(m_color)); }
        std::tuple<ConsoleColor, ConsoleColor> m_color;
    };

    // Prints prompt and blocks waiting for a line of text to be entered
    std::string ConsoleReadLine(const char* prompt);

    std::optional<std::string> OpenFileDialog(const char* title = "Open",
                                              const char* filterName = "All files",
                                              const char* filterTypes = "*.*",
                                              std::optional<fs::path> initialPath = {});

    bool ExecuteShellCommand(const char* command);

    // Block until debugger attaches to process
    void WaitForDebuggerAttach(bool breakOnAttach = false);

} // namespace Platform
