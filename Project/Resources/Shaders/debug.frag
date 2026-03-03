#version 330 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 BloomEmission;

uniform vec3 debugColor;

void main()
{
    FragColor = vec4(debugColor, 1.0);
    BloomEmission = vec4(0.0);
}