R"ShaderSource(
#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D inputTexture;

void main() {
    color = texture( inputTexture, UV ).rgb;
}
)ShaderSource"
