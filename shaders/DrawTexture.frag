#version 330 core

in vec2 UV;
out vec3 color;

uniform sampler2D renderedTexture;
uniform sampler2D overlayTexture;
uniform float overlayAlpha;

void main() {
    vec3 c1 = texture( renderedTexture, UV ).rgb;
    vec4 c2 = texture( overlayTexture, UV ).rgba;
    //float overlayAlpha = 0.9;
    float ratio = c2.a - (1 - overlayAlpha);
    color = mix(c1, vec3(c2), ratio);
}
