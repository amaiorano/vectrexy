#include "emulator/Screen.h"
#include "core/Gui.h"
#include "emulator/EngineTypes.h"

namespace {
    //@TODO: make these conditionally const for "shipping" build
    int32_t RampUpDelay = 5;
    int32_t RampDownDelay = 10;
    int32_t VelocityXDelay = 6;
    // LineDrawScale is required because introducing ramp and velX delays means we now create lines
    // that go outside the 256x256 grid. So we scale down the line drawing values a little to make
    // it fit within the grid again.
    float LineDrawScale = 0.85f;
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

    case RampPhase::RampOff:
    case RampPhase::RampOn:
        break;
    }

    const auto lastPos = m_pos;
    const Vector2 currDir = Normalized({m_velocityX, m_velocityY});

    // Move beam while ramp is on or its way down
    switch (m_rampPhase) {
    case RampPhase::RampDown:
    case RampPhase::RampOn: {
        const auto offset = Vector2{m_xyOffset, m_xyOffset};
        Vector2 velocity{m_velocityX, m_velocityY};
        Vector2 delta = (velocity + offset) / 128.f * static_cast<float>(cycles) * LineDrawScale;
        m_pos += delta;
        break;
    }

    case RampPhase::RampOff:
    case RampPhase::RampUp:
        break;
    }

    // We might draw even when integrators are disabled (e.g. drawing dots)
    bool drawingEnabled = !m_blank && (m_brightness > 0.f && m_brightness <= 128.f);
    if (drawingEnabled) {
        if (m_lastDrawingEnabled && (Magnitude(m_lastDir) > 0.f) && (m_lastDir == currDir) &&
            !renderContext.lines.empty()) {
            renderContext.lines.back().p1 = m_pos;
        } else {
            auto lerp = [](float a, float b, float t) { return a + t * (b - a); };
            auto easeOut = [](float v) { return 1.f - powf(1.f - v, 5); };

            // Lerp between the linear brightness value and an ease out curve based on the user-set
            // brightness curve value.
            float b = m_brightness / 128.f;
            b = lerp(b, easeOut(b), m_brightnessCurve);
            renderContext.lines.emplace_back(Line{lastPos, m_pos, b});
        }
    }

    m_lastDrawingEnabled = drawingEnabled;
    m_lastDir = currDir;
}

void Screen::FrameUpdate(double /*frameTime*/) {
    static bool ScreenImGui = false;
    IMGUI_CALL(Debug, ImGui::Checkbox("<<< Screen >>>", &ScreenImGui));

    IMGUI_CALL_IF(ScreenImGui, Debug, ImGui::SliderInt("RampUpDelay", &RampUpDelay, 0, 20));
    IMGUI_CALL_IF(ScreenImGui, Debug, ImGui::SliderInt("RampDownDelay", &RampDownDelay, 0, 20));
    IMGUI_CALL_IF(ScreenImGui, Debug, ImGui::SliderInt("VelocityXDelay", &VelocityXDelay, 0, 30));
    IMGUI_CALL_IF(ScreenImGui, Debug,
                  ImGui::SliderFloat("LineDrawScale", &LineDrawScale, 0.1f, 1.f));
    m_velocityX.CyclesToUpdateValue = VelocityXDelay;
}

void Screen::ZeroBeam() {
    //@TODO: move beam towards 0,0 over time
    m_pos = {0.f, 0.f};
    m_lastDrawingEnabled = false;
}
