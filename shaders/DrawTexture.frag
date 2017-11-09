#version 330 core

in vec2 UV;
out vec4 color;

uniform sampler2D renderedTexture;
//uniform sampler2D overlayTexture;
uniform float overlayAlpha;

void main() {
    //vec2 scaledUV = (UV - vec2(0.5)) / 0.8 + vec2(0.5);
    //vec3 c1 = texture(renderedTexture, scaledUV).rgb;

    //vec3 c1 = texture( renderedTexture, UV ).rgb;
    //vec4 c2 = texture( overlayTexture, UV ).rgba;
    //float overlayAlpha = 0.9;
    //float ratio = max(0, c2.a - (1 - overlayAlpha));
    //color = mix(c1, vec3(c2), ratio);

    color = texture( renderedTexture, UV ).rgba;
}
