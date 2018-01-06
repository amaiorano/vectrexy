R"ShaderSource(
#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D crtTexture;
uniform sampler2D overlayTexture;
uniform float overlayAlpha;

void main() {
    vec3 c1 = texture( crtTexture, UV ).rgb;
    vec4 c2 = texture( overlayTexture, UV ).rgba;
    float ratio = max(0, c2.a - (1 - overlayAlpha));
    color = mix(c1, vec3(c2), ratio);
}
)ShaderSource"
