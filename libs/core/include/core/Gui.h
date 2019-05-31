#pragma once

#ifndef DEBUG_UI_ENABLED

#define IMGUI_CALL(window, func)
#define IMGUI_CALL_IF(condition, window, func)

#else

#include <array>
#include <imgui.h>

namespace Gui {
    namespace Window {
        enum Type { Debug, Size };
    }

    inline std::array<bool, Window::Size> EnabledWindows = {};

    namespace Internal {
        template <typename Func>
        void DoImguiCall(const char* name, Window::Type type, Func func) {
            if (Gui::EnabledWindows[type]) {
                if (strcmp(name, "Debug") == 0)
                    name = "Debug Options"; // Don't collide with ImGui's internal window
                bool open = true;
                ImGui::Begin(name, &open);
                func();
                if (!open)
                    EnabledWindows[type] = false;
                ImGui::End();
            }
        }
    } // namespace Internal

#define IMGUI_CALL(window, func)                                                                   \
    Gui::Internal::DoImguiCall(#window, Gui::Window::window, [&] { func; })

#define IMGUI_CALL_IF(condition, window, func)                                                     \
    if (condition)                                                                                 \
    Gui::Internal::DoImguiCall(#window, Gui::Window::window, [&] { func; })

} // namespace Gui

#endif
