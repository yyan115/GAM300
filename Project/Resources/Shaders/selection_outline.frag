#version 330 core
out vec4 FragColor;

uniform vec4 outlineColor;

void main()
{
    FragColor = outlineColor;
}
