#version 330 core

// Ouput data
layout(location = 0) out vec3 color;

void main(){
    // For now, just white. Will need to pass in a vertex color value.
    color = vec3(1);
}
