R"ShaderSource(
#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D renderedTexture;
uniform float darkenRatio;

void main() {
    color = mix(texture(renderedTexture, UV).rgb, vec3(0), darkenRatio);
}
)ShaderSource"
