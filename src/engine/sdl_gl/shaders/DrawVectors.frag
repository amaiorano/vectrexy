R"ShaderSource(
#version 330 core

in float vertexBrightness;
out vec3 color;

void main() {
    // For now, just white. Will need to pass in a vertex color value.
    color = vec3(vertexBrightness);
}
)ShaderSource"
