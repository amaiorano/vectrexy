#include "InputDevice.h"
#include "engine/EngineClient.h"

// Polls inputDevice using inputMapping and populates relevant section of input
void PollInputDevice(const InputDevice& inputDevice, const InputMapping& inputMapping,
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

        uint8_t joystickIndex = kb->joystickIndex;

        input.SetButton(joystickIndex, 0, IsKeyDown(KeyboardInputMapping::B1));
        input.SetButton(joystickIndex, 1, IsKeyDown(KeyboardInputMapping::B2));
        input.SetButton(joystickIndex, 2, IsKeyDown(KeyboardInputMapping::B3));
        input.SetButton(joystickIndex, 3, IsKeyDown(KeyboardInputMapping::B4));

        input.SetAnalogAxisX(joystickIndex,
                             remapDigitalToAxisValue(IsKeyDown(KeyboardInputMapping::Left),
                                                     IsKeyDown(KeyboardInputMapping::Right)));
        input.SetAnalogAxisY(joystickIndex,
                             remapDigitalToAxisValue(IsKeyDown(KeyboardInputMapping::Up),
                                                     IsKeyDown(KeyboardInputMapping::Down)));

    } else if (auto gp = std::get_if<GamepadInputDevice>(&inputDevice)) {
        auto remapAxisValue = [](int32_t value) -> int8_t {
            return static_cast<int8_t>((value / 32767.0f) * 127);
        };

        uint8_t joystickIndex = gp->joystickIndex;
        auto& controller = gp->driver.get().ControllerByIndex(joystickIndex);

        auto IsButtonDown = [&](GamepadInputMapping::Button button) {
            return controller.GetButtonState(button).down;
        };

        input.SetButton(joystickIndex, 0, IsButtonDown(GamepadInputMapping::B1));
        input.SetButton(joystickIndex, 1, IsButtonDown(GamepadInputMapping::B2));
        input.SetButton(joystickIndex, 2, IsButtonDown(GamepadInputMapping::B3));
        input.SetButton(joystickIndex, 3, IsButtonDown(GamepadInputMapping::B4));

        if (IsButtonDown(GamepadInputMapping::Left) || IsButtonDown(GamepadInputMapping::Right)) {
            input.SetAnalogAxisX(joystickIndex,
                                 remapDigitalToAxisValue(IsButtonDown(GamepadInputMapping::Left),
                                                         IsButtonDown(GamepadInputMapping::Right)));
        } else {
            input.SetAnalogAxisX(
                joystickIndex, remapAxisValue(controller.GetAxisValue(GamepadInputMapping::AxisX)));
        }

        if (IsButtonDown(GamepadInputMapping::Down) || IsButtonDown(GamepadInputMapping::Up)) {
            input.SetAnalogAxisX(joystickIndex,
                                 remapDigitalToAxisValue(IsButtonDown(GamepadInputMapping::Down),
                                                         IsButtonDown(GamepadInputMapping::Up)));
        } else {
            input.SetAnalogAxisX(joystickIndex, -remapAxisValue(controller.GetAxisValue(
                                                    GamepadInputMapping::AxisY)));
        }
    }
}
