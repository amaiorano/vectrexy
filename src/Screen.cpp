#include "Screen.h"
#include "EngineClient.h"
#include <imgui.h>

namespace {
    //@TODO: make these conditionally const for "shipping" build
    static int32_t RampUpDelay = 5;
    static int32_t RampDownDelay = 10;
    static int32_t VelocityXDelay = 6;
} // namespace

void Screen::Init() {
    m_velocityX.CyclesToUpdateValue = VelocityXDelay;
}

void Screen::Update(cycles_t cycles, RenderContext& renderContext) {
    m_velocityX.Update(cycles);
    m_velocityY.Update(cycles);

    // Handle switching to RampUp/RampDown
    switch (m_rampPhase) {
    case RampPhase::RampOff:
    case RampPhase::RampDown:
        if (m_integratorsEnabled) {
            m_rampPhase = RampPhase::RampUp;
            m_rampDelay = RampUpDelay;
        }
        break;

    case RampPhase::RampOn:
    case RampPhase::RampUp:
        if (!m_integratorsEnabled) {
            m_rampPhase = RampPhase::RampDown;
            m_rampDelay = RampDownDelay;
        }
    }

    // Handle switching to RampOn/RampOff
    switch (m_rampPhase) {
    case RampPhase::RampUp:
        // Wait some cycles, then go to RampOn
        if (--m_rampDelay <= 0) {
            m_rampPhase = RampPhase::RampOn;
        }
        break;

    case RampPhase::RampDown:
        // Wait some cycles, then go to RampOff
        if (--m_rampDelay <= 0) {
            m_rampPhase = RampPhase::RampOff;
        }
    }

    const auto lastPos = m_pos;

    // Move beam while ramp is on or its way down
    switch (m_rampPhase) {
    case RampPhase::RampDown:
    case RampPhase::RampOn: {
        const auto offset = Vector2{m_xyOffset, m_xyOffset};
        Vector2 velocity{m_velocityX, m_velocityY};
        Vector2 delta = (velocity + offset) / 128.f * static_cast<float>(cycles);
        m_pos += delta;
        m_pos.x = std::clamp(m_pos.x, -128.f, 127.f);
        m_pos.y = std::clamp(m_pos.y, -128.f, 127.f);
        break;
    }
    }

    // We might draw even when integrators are disabled (e.g. drawing dots)
    bool drawingEnabled = !m_blank && (m_brightness > 0.f && m_brightness <= 128.f);
    if (drawingEnabled) {
        renderContext.lines.emplace_back(Line{lastPos, m_pos});
    }
}

void Screen::FrameUpdate() {
    ImGui::SliderInt("RampUpDelay", &RampUpDelay, 0, 20);
    ImGui::SliderInt("RampDownDelay", &RampDownDelay, 0, 20);
    ImGui::SliderInt("VelocityXDelay", &VelocityXDelay, 0, 30);
    m_velocityX.CyclesToUpdateValue = VelocityXDelay;
}

void Screen::ZeroBeam() {
    //@TODO: move beam towards 0,0 over time
    m_pos = {0.f, 0.f};
}
