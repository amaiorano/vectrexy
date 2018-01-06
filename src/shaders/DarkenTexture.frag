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

    //vec3 colorMS = texture( renderedTexture, UV ).rgb * 1000.0;
    //vec3 finalColorMS = max(colorMS - vec3(frameTime), vec3(0));
    //vec3 finalColorMS = mix(colorMS.rgb, vec3(0), vec3(frameTime));
    //vec3 finalColor = finalColorMS / 1000.0;

    //color = max(texture( renderedTexture, UV ).rgb - vec3(frameTime * darkenSpeedScale), vec3(0));


    vec3 current = texture(renderedTexture, UV).rgb;
    vec3 target = vec3(0);
    float rate = 0.99;
    color = integrateDamped(current, target, rate, frameTime * darkenSpeedScale);


    //@HACK: We target value below 0 enough to account for float precision problems.
    // We clamp to 0 below.
    ///vec3 source = texture( renderedTexture, UV ).rgb;
    /////vec3 target = vec3(-0.3);
    ///vec3 target = vec3(0);
    ///float fadeRate = 0.99;
    ///float speedScale = 5;
    ///float ratio = 1.0 - pow(1.0 - fadeRate, frameTime * speedScale);
    ///vec3 finalColor = mix(source.rgb, target, ratio);
    /////vec3 finalColor = source.rgb + (target - source.rgb) * ratio;
    /////finalColor = max(finalColor, vec3(0)); // Clamp to 0
    ///color = finalColor;
}
)ShaderSource"
