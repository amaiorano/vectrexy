#pragma once

#include <SDL_gamecontroller.h>
#include <SDL_scancode.h>
#include <array>

struct KeyboardInputMapping {
    enum Type { Up, Down, Left, Right, B1, B2, B3, B4, Count };

    constexpr static std::array<const char*, Count> Name = {
        "Joystick Up", "Joystick Down", "Joystick Left", "Joystick Right",
        "Button 1",    "Button 2",      "Button 3",      "Button 4"};

    using ScanCodeArray = std::array<SDL_Scancode, Count>;

    constexpr static ScanCodeArray DefaultKeysPlayer1 = {
        SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_A,  SDL_SCANCODE_S,    SDL_SCANCODE_D,    SDL_SCANCODE_F};

    constexpr static ScanCodeArray DefaultKeysPlayer2 = {
        SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
        SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V};

    constexpr static std::array<ScanCodeArray, 2> DefaultKeys = {DefaultKeysPlayer1,
                                                                 DefaultKeysPlayer2};

    // Current keyboard mapping
    using KeyToScancodeArray = std::array<SDL_Scancode, Count>;
    KeyToScancodeArray keys = DefaultKeysPlayer1;
};

struct GamepadInputMapping {
    enum Axis { AxisX, AxisY, AxisCount };
    enum Button { Up, Down, Left, Right, B1, B2, B3, B4, ButtonCount };

    constexpr static std::array<const char*, AxisCount> AxisName = {"Horizontal Axis (X)",
                                                                    "Vertical Axis (Y)"};

    constexpr static std::array<const char*, ButtonCount> ButtonName = {
        "Joystick Up", "Joystick Down", "Joystick Left", "Joystick Right",
        "Button 1",    "Button 2",      "Button 3",      "Button 4"};

    constexpr static std::array<SDL_GameControllerAxis, AxisCount> DefaultAxes = {
        SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY};

    constexpr static std::array<SDL_GameControllerButton, ButtonCount> DefaultButtons = {
        SDL_CONTROLLER_BUTTON_DPAD_UP,   SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_X,         SDL_CONTROLLER_BUTTON_A,
        SDL_CONTROLLER_BUTTON_B,         SDL_CONTROLLER_BUTTON_Y};

    // Current game controller mapping
    std::array<SDL_GameControllerAxis, AxisCount> axes = DefaultAxes;
    std::array<SDL_GameControllerButton, ButtonCount> buttons = DefaultButtons;
};

// Each player has an InputMapping for all possible devices. We do this so that the user can switch
// between devices without losing their mapping. For example, if player 1 sets up custom keyboard
// keys, then switches to gamepad, then back to keyboard, we want to keep the previously set
// keyboard keys.
struct InputMapping {
    KeyboardInputMapping keyboard;
    GamepadInputMapping gamepad;
};
