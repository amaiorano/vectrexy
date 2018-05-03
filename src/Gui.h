#pragma once

#include <imgui.h>

namespace Gui {
    namespace Window {
        enum Type { Debug, Size };
    }

    inline bool EnabledWindows[Window::Size] = {};

#define IMGUI_CALL(window, func)                                                                   \
    if (Gui::EnabledWindows[Gui::Window::##window]) {                                              \
        ImGui::Begin(#window);                                                                     \
        func;                                                                                      \
        ImGui::End();                                                                              \
    }

} // namespace Gui
