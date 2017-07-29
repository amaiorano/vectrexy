#pragma once

#include <functional>
#include <tuple>

namespace Platform {

    void SetConsoleTitle(const char* title);

    // Set handler for Ctrl+C. Handler must return true if handled, or false to allow default system
    // handler. Pass in nullptr to unset handler.
    void SetConsoleCtrlHandler(std::function<bool()> handler);

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

} // namespace Platform
