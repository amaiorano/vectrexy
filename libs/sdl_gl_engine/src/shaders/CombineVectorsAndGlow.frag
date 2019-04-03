R"ShaderSource(
#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D vectorsTexture;
uniform sampler2D glowTexture;

void main() {
    vec3 c1 = texture( vectorsTexture, UV ).rgb;
    vec3 c2 = texture( glowTexture, UV ).rgb;
    color = max(c1, c2);
}
)ShaderSource"
