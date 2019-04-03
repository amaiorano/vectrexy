R"ShaderSource(
#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D renderedTexture;
uniform float frameTime;
uniform float darkenSpeedScale;

// Integate current to target using a damped approach. Rate of 0.99 means we'd reach
// 99% of the way to target in 1 second.
vec3 integrateDamped(vec3 current, vec3 target, float rate, float deltaTime) {
    float ratio = 1.0 - pow(1.0 - rate, deltaTime);
    return mix(current, target, ratio);
}

void main() {
    vec3 current = texture(renderedTexture, UV).rgb;
    if (any(greaterThan(current, vec3(0.1)))) {
        float rate = 0.99;
        vec3 target = vec3(0);
        color = integrateDamped(current, target, rate, frameTime * darkenSpeedScale);
    } else {
        color = vec3(0);
    }
}
)ShaderSource"
