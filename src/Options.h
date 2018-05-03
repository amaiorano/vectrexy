#pragma once

#include <optional>

struct Options {
    std::optional<int> windowX;
    std::optional<int> windowY;
    std::optional<int> windowWidth;
    std::optional<int> windowHeight;
    std::optional<bool> imguiDebugWindow;
    std::optional<float> imguiFontScale;
};

Options LoadOptionsFile(const char* file);
