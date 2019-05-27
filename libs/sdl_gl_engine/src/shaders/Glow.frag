R"ShaderSource(
#version 330 core

in vec2 UV;

out vec3 color;

uniform sampler2D inputTexture;
uniform float resolution;
uniform float radius;
uniform vec2 dir;

void main() {
    vec4 sum = vec4(0.0);
    
    // the amount to blur, i.e. how far off center to sample from 
    // 1.0 -> blur by one pixel
    // 2.0 -> blur by two pixels, eUV.
    float blur = radius / resolution; 

    // the direction of our blur
    // (1.0, 0.0) -> x-axis blur
    // (0.0, 1.0) -> y-axis blur
    float hstep = dir.x * blur;
    float vstep = dir.y * blur;

    // Apply blurring, using a 9-tap filter with predefined gaussian weights

    sum += texture(inputTexture, vec2(UV.x - 4.0 * hstep, UV.y - 4.0 * vstep)) * 0.0162162162;
    sum += texture(inputTexture, vec2(UV.x - 3.0 * hstep, UV.y - 3.0 * vstep)) * 0.0540540541;
    sum += texture(inputTexture, vec2(UV.x - 2.0 * hstep, UV.y - 2.0 * vstep)) * 0.1216216216;
    sum += texture(inputTexture, vec2(UV.x - 1.0 * hstep, UV.y - 1.0 * vstep)) * 0.1945945946;
    
    sum += texture(inputTexture, vec2(UV.x, UV.y)) * 0.2270270270;

    sum += texture(inputTexture, vec2(UV.x + 1.0 * hstep, UV.y + 1.0 * vstep)) * 0.1945945946;
    sum += texture(inputTexture, vec2(UV.x + 2.0 * hstep, UV.y + 2.0 * vstep)) * 0.1216216216;
    sum += texture(inputTexture, vec2(UV.x + 3.0 * hstep, UV.y + 3.0 * vstep)) * 0.0540540541;
    sum += texture(inputTexture, vec2(UV.x + 4.0 * hstep, UV.y + 4.0 * vstep)) * 0.0162162162;

    color = sum.rgb;
}
)ShaderSource"
