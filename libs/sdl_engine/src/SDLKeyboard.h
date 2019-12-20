#pragma once

#include <SDL.h>
#include <algorithm>
#include <array>
#include <optional>

class SDLKeyboard {
public:
    struct KeyState {
        bool down = false;
        bool pressed = false;
    };

    void OnKeyStateChange(const SDL_KeyboardEvent& keyboardEvent) {
        auto& keyState = m_keyStates[keyboardEvent.keysym.scancode];
        if (keyboardEvent.type == SDL_KEYDOWN) {
            keyState.pressed = !keyState.down;
            keyState.down = true;
        } else {
            keyState.down = false;
        }
    }

    void PostFrameUpdateKeyStates() {
        for (auto& keyState : m_keyStates) {
            keyState.pressed = false;
        }
    }

    const KeyState& GetKeyState(SDL_Scancode scancode) { return m_keyStates[scancode]; }

    void ResetKeyState(SDL_Scancode scancode) { m_keyStates[scancode] = {}; }

    // Returns the last key that was recorded as pressed. If more than one, will only return the
    // first one in scancode order.
    std::optional<SDL_Scancode> GetLastKeyPressed() {
        auto iter = std::find_if(m_keyStates.begin(), m_keyStates.end(),
                                 [](KeyState& state) { return state.pressed; });

        if (iter == m_keyStates.end())
            return {};

        return static_cast<SDL_Scancode>(iter - m_keyStates.begin());
    }

private:
    std::array<KeyState, SDL_NUM_SCANCODES> m_keyStates;
};
