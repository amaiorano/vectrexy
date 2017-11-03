#version 330 core

in vec2 UV;

out vec3 color;

uniform sampler2D renderedTexture;
uniform float time;

void main() {
    color = texture( renderedTexture, UV ).xyz;
}
