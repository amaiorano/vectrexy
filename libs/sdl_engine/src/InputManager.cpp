#include "InputManager.h"

#include "core/Base.h"
#include "emulator/EngineTypes.h"
#include "engine/Options.h"

namespace {
    // Convert vector<U> to array<size, T> where U is explicitly convertible to T
    template <typename T, int Size, typename U>
    std::array<T, Size> ToArray(const std::vector<U> vec) {
        static_assert(std::is_convertible_v<T, U>);
        assert(vec.size() == Size);
        std::array<T, Size> arr{};
        for (size_t i = 0; i < vec.size(); ++i) {
            arr[i] = static_cast<T>(vec[i]);
        }
        return arr;
    }

    // Convert array<U, Size> to vector<T> where U is explicitly convertible to T
    template <typename T, typename U, size_t Size>
    std::vector<T> ToVector(const std::array<U, Size> arr) {
        // static_assert(std::is_convertible_v<T, U>);
        std::vector<T> vec;
        vec.reserve(Size);
        for (auto& v : arr) {
            vec.push_back(static_cast<T>(v));
        }
        return vec;
    }

    const char* OptionNameKeyboardKeys = "inputKeys%d";
    const char* OptionNameGamepadAxes = "inputAxes%d";
    const char* OptionNameGamepadButtons = "inputButtons%d";

    void AddInputMappingOptions(Options& options, const InputMapping& mapping, int player) {
        options.Add<std::vector<int>>(
            FormattedString<>(OptionNameKeyboardKeys, player),
            std::vector<int>(mapping.keyboard.keys.begin(), mapping.keyboard.keys.end()));

        options.Add<std::vector<int>>(
            FormattedString<>(OptionNameGamepadAxes, player),
            std::vector<int>(mapping.gamepad.axes.begin(), mapping.gamepad.axes.end()));

        options.Add<std::vector<int>>(
            FormattedString<>(OptionNameGamepadButtons, player),
            std::vector<int>(mapping.gamepad.buttons.begin(), mapping.gamepad.buttons.end()));
    }

    void SetInputMappingOptions(Options& options, const InputMapping& mapping, int player) {
        options.Set<std::vector<int>>(
            FormattedString<>(OptionNameKeyboardKeys, player),
            std::vector<int>(mapping.keyboard.keys.begin(), mapping.keyboard.keys.end()));

        options.Set<std::vector<int>>(
            FormattedString<>(OptionNameGamepadAxes, player),
            std::vector<int>(mapping.gamepad.axes.begin(), mapping.gamepad.axes.end()));

        options.Set<std::vector<int>>(
            FormattedString<>(OptionNameGamepadButtons, player),
            std::vector<int>(mapping.gamepad.buttons.begin(), mapping.gamepad.buttons.end()));
    }

    InputMapping GetInputMappingOptions(const Options& options, int player) {
        InputMapping mapping;
        mapping.keyboard.keys = ToArray<SDL_Scancode, KeyboardInputMapping::Count>(
            options.Get<std::vector<int>>(FormattedString<>(OptionNameKeyboardKeys, player)));
        mapping.gamepad.axes = ToArray<SDL_GameControllerAxis, GamepadInputMapping::AxisCount>(
            options.Get<std::vector<int>>(FormattedString(OptionNameGamepadAxes, player)));
        mapping.gamepad.buttons =
            ToArray<SDL_GameControllerButton, GamepadInputMapping::ButtonCount>(
                options.Get<std::vector<int>>(FormattedString<>(OptionNameGamepadButtons, player)));
        return mapping;
    }

    // Polls inputDevice using inputMapping and populates relevant section of input
    void PollInputDevice(const InputDevice::Type& inputDevice, const InputMapping& inputMapping,
                         Input& input) {

        auto remapDigitalToAxisValue = [](auto left, auto right) -> int8_t {
            return left ? -128 : right ? 127 : 0;
        };

        if (auto nd = std::get_if<NullInputDevice>(&inputDevice)) {
            // Nothing to update

        } else if (auto kb = std::get_if<KeyboardInputDevice>(&inputDevice)) {
            auto IsKeyDown = [&](KeyboardInputMapping::Type type) -> bool {
                return kb->keyboard.get().GetKeyState(inputMapping.keyboard.keys[type]).down;
            };

            uint8_t joystickIndex = checked_static_cast<uint8_t>(kb->joystickIndex);

            input.SetButton(joystickIndex, 0, IsKeyDown(KeyboardInputMapping::B1));
            input.SetButton(joystickIndex, 1, IsKeyDown(KeyboardInputMapping::B2));
            input.SetButton(joystickIndex, 2, IsKeyDown(KeyboardInputMapping::B3));
            input.SetButton(joystickIndex, 3, IsKeyDown(KeyboardInputMapping::B4));

            input.SetAnalogAxisX(joystickIndex,
                                 remapDigitalToAxisValue(IsKeyDown(KeyboardInputMapping::Left),
                                                         IsKeyDown(KeyboardInputMapping::Right)));
            input.SetAnalogAxisY(joystickIndex,
                                 remapDigitalToAxisValue(IsKeyDown(KeyboardInputMapping::Down),
                                                         IsKeyDown(KeyboardInputMapping::Up)));

        } else if (auto gp = std::get_if<GamepadInputDevice>(&inputDevice)) {
            auto remapAxisValue = [](int32_t value) -> int8_t {
                return static_cast<int8_t>((value / 32767.0f) * 127);
            };

            uint8_t joystickIndex = checked_static_cast<uint8_t>(gp->joystickIndex);
            auto& controller = gp->driver.get().ControllerByIndex(joystickIndex);

            auto IsButtonDown = [&](GamepadInputMapping::Button button) {
                return controller.GetButtonState(button).down;
            };

            input.SetButton(joystickIndex, 0, IsButtonDown(GamepadInputMapping::B1));
            input.SetButton(joystickIndex, 1, IsButtonDown(GamepadInputMapping::B2));
            input.SetButton(joystickIndex, 2, IsButtonDown(GamepadInputMapping::B3));
            input.SetButton(joystickIndex, 3, IsButtonDown(GamepadInputMapping::B4));

            if (IsButtonDown(GamepadInputMapping::Left) ||
                IsButtonDown(GamepadInputMapping::Right)) {
                input.SetAnalogAxisX(joystickIndex, remapDigitalToAxisValue(
                                                        IsButtonDown(GamepadInputMapping::Left),
                                                        IsButtonDown(GamepadInputMapping::Right)));
            } else {
                input.SetAnalogAxisX(joystickIndex, remapAxisValue(controller.GetAxisValue(
                                                        GamepadInputMapping::AxisX)));
            }

            if (IsButtonDown(GamepadInputMapping::Down) || IsButtonDown(GamepadInputMapping::Up)) {
                input.SetAnalogAxisX(
                    joystickIndex, remapDigitalToAxisValue(IsButtonDown(GamepadInputMapping::Down),
                                                           IsButtonDown(GamepadInputMapping::Up)));
            } else {
                input.SetAnalogAxisX(joystickIndex, -remapAxisValue(controller.GetAxisValue(
                                                        GamepadInputMapping::AxisY)));
            }
        }
    }

} // namespace

void InputManager::Init(SDLKeyboard& keyboard, SDLGameControllerDriver& controllerDriver) {
    m_keyboard = &keyboard;
    m_controllerDriver = &controllerDriver;

    // Keyboard is the only shared device, so make sure each player has a unique set of default
    // keys.
    m_inputMappings[0].keyboard.keys = KeyboardInputMapping::DefaultKeys[0];
    m_inputMappings[1].keyboard.keys = KeyboardInputMapping::DefaultKeys[1];
}

void InputManager::AddOptions(Options& options) {
    AddInputMappingOptions(options, m_inputMappings[0], 0);
    AddInputMappingOptions(options, m_inputMappings[1], 1);
    options.Add<int>("inputDeviceIndex0", (int)InputDevice::IndexOf<KeyboardInputDevice>);
    options.Add<int>("inputDeviceIndex1", (int)InputDevice::IndexOf<NullInputDevice>);
}

void InputManager::ReadOptions(Options& options) {
    m_inputMappings[0] = GetInputMappingOptions(options, 0);
    m_inputMappings[1] = GetInputMappingOptions(options, 1);
    SetInputDeviceByIndex(options.Get<int>("inputDeviceIndex0"), 0);
    SetInputDeviceByIndex(options.Get<int>("inputDeviceIndex1"), 1);
}

void InputManager::WriteOptions(Options& options) {
    SetInputMappingOptions(options, m_inputMappings[0], 0);
    SetInputMappingOptions(options, m_inputMappings[1], 1);
    options.Set("inputDeviceIndex0", static_cast<int>(m_inputDevices[0].index()));
    options.Set("inputDeviceIndex1", static_cast<int>(m_inputDevices[1].index()));
}

Input InputManager::Poll() {
    Input input{};
    PollInputDevice(m_inputDevices[0], m_inputMappings[0], input);
    PollInputDevice(m_inputDevices[1], m_inputMappings[1], input);
    return input;
}

void InputManager::SetInputDeviceByIndex(size_t inputDeviceIndex, int player) {
    switch (inputDeviceIndex) {
    case InputDevice::IndexOf<NullInputDevice>:
        SetInputDevice<NullInputDevice>(player);
        break;
    case InputDevice::IndexOf<KeyboardInputDevice>:
        SetInputDevice<KeyboardInputDevice>(player);
        break;

    case InputDevice::IndexOf<GamepadInputDevice>:
        SetInputDevice<GamepadInputDevice>(player);
        break;
    default:
        assert(false);
    }
}
