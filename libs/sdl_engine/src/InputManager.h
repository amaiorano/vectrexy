#pragma once

#include "InputDevice.h"
#include <array>

class Options;
class SDLKeyboard;
class SDLGameControllerDriver;

class InputManager {
public:
    void Init(SDLKeyboard& keyboard, SDLGameControllerDriver& controllerDriver);

    void AddOptions(Options& options);
    void ReadOptions(Options& options);
    void WriteOptions(Options& options);

    InputDevice::Type& Device(int player) { return m_inputDevices[player]; }
    InputMapping& Mapping(int player) { return m_inputMappings[player]; }
    int& DeviceIndex(int player) { return m_inputDeviceIndices[player]; }

    void SetInputDeviceByIndex(size_t inputDeviceIndex, int player);

    // Polls current input devices using current mapping for each player, and returns an Input
    // object representing the state
    Input Poll();

private:
    template <typename InputDeviceType>
    void SetInputDevice(int player) {
        if constexpr (std::is_same_v<InputDeviceType, NullInputDevice>) {
            m_inputDevices[player] = NullInputDevice{};
        } else if constexpr (std::is_same_v<InputDeviceType, KeyboardInputDevice>) {
            m_inputDevices[player] = KeyboardInputDevice{*m_keyboard, player};
        } else if constexpr (std::is_same_v<InputDeviceType, GamepadInputDevice>) {
            m_inputDevices[player] = GamepadInputDevice{*m_controllerDriver, player};
        } else {
            static_assert(false);
        }
    }

    SDLKeyboard* m_keyboard{};
    SDLGameControllerDriver* m_controllerDriver{};

    static constexpr int NumPlayers = 2;
    std::array<InputMapping, NumPlayers> m_inputMappings;
    std::array<InputDevice::Type, NumPlayers> m_inputDevices;
    std::array<int, NumPlayers> m_inputDeviceIndices = {0, 0};
};
