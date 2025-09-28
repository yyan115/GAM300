#version 300 es
out vec4 FragColor;

uniform vec3 debugColor;

void main()
{
    FragColor = vec4(debugColor, 1.0);
}