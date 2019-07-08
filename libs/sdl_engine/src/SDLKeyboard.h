#pragma once

#include <SDL.h>
#include <array>

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

private:
    std::array<KeyState, SDL_NUM_SCANCODES> m_keyStates;
};
