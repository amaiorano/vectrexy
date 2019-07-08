#include "SDLGameController.h"
#include <cassert>
#include <core/ConsoleOutput.h>

void SDLGameControllerDriver::PostFrameUpdateKeyStates() {
    for (auto& kvp : m_playerIndexToGamepad) {
        kvp.second.PostFrameUpdateStates();
    }
}

void SDLGameControllerDriver::AddController(int index) {
    if (index >= 2) {
        Printf("Cannot support more than 2 gamepads\n");
        return;
    }

    if (SDL_IsGameController(index)) {
        SDL_GameController* controller = SDL_GameControllerOpen(index);
        if (controller) {
            auto joy = SDL_GameControllerGetJoystick(controller);
            auto instanceId = SDL_JoystickInstanceID(joy);
            m_instanceIdToPlayerIndex[instanceId] = index;
            m_playerIndexToGamepad[index] = {};
        }
    }
}

void SDLGameControllerDriver::RemoveController(int instanceId) {
    auto controller = SDL_GameControllerFromInstanceID(instanceId);
    SDL_GameControllerClose(controller);

    int index = m_instanceIdToPlayerIndex[instanceId];
    auto iter = m_playerIndexToGamepad.find(index);
    assert(iter != m_playerIndexToGamepad.end());
    m_playerIndexToGamepad.erase(iter);
}

int SDLGameControllerDriver::NumControllers() const {
    return static_cast<int>(m_playerIndexToGamepad.size());
}

bool SDLGameControllerDriver::IsControllerConnected(int index) const {
    return m_playerIndexToGamepad.find(index) != m_playerIndexToGamepad.end();
}

GameController& SDLGameControllerDriver::ControllerByInstanceId(int instanceId) {
    assert(m_instanceIdToPlayerIndex.find(instanceId) != m_instanceIdToPlayerIndex.end());
    int playerIndex = m_instanceIdToPlayerIndex[instanceId];
    auto iter = m_playerIndexToGamepad.find(playerIndex);
    assert(iter != m_playerIndexToGamepad.end());
    return iter->second;
}

const GameController& SDLGameControllerDriver::ControllerByIndex(int index) const {
    auto iter = m_playerIndexToGamepad.find(index);
    assert(iter != m_playerIndexToGamepad.end());
    return iter->second;
}
