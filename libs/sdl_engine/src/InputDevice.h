#pragma once

#include "InputMapping.h"
#include "SDLGameController.h"
#include "SDLKeyboard.h"
#include "core/StdUtil.h"
#include <array>
#include <functional>
#include <type_traits>

class Input;

// InputDevice types
// Each player has a device they are using, and an InputMapping for that device

struct NullInputDevice {};

struct KeyboardInputDevice {
    std::reference_wrapper<SDLKeyboard> keyboard;
    int joystickIndex;
};

struct GamepadInputDevice {
    std::reference_wrapper<SDLGameControllerDriver> driver;
    int joystickIndex;
};

namespace InputDevice {
    using Type = std::variant<NullInputDevice, KeyboardInputDevice, GamepadInputDevice>;

    constexpr std::array<const char*, std::variant_size_v<Type>> Name = {"None", "Keyboard",
                                                                         "Gamepad"};

    template <typename DeviceType>
    constexpr size_t IndexOf = std_util::variant_type_index<DeviceType, Type>::value;

} // namespace InputDevice
