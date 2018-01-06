R"ShaderSource(
#version 330 core

layout(location = 0) in vec3 vertexPosition_clipspace;
layout(location = 1) in vec2 vertexUV;

out vec2 UV;

uniform mat4 MVP;

void main(){
    gl_Position = vec4(vertexPosition_clipspace,1);
    UV = vertexUV;
}
)ShaderSource"
