#version 300 es
layout (location = 0) in vec3 aPos;

layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
    mat4 viewProjection;
};

uniform mat4 model;

void main()
{
	gl_Position = viewProjection * model * vec4(aPos, 1.0);
}
