#version 330 core

layout (location = 0) in vec3 aPos;

out vec3 FragPos;       // World-space position of the volume surface
out vec3 LocalPos;      // Object-space position [0,1] range for sampling

layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
};

uniform mat4 model;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    // Convert from [-0.5, 0.5] to [0, 1] for clean sampling in frag shader
    LocalPos = aPos + 0.5;

    gl_Position = projection * view * worldPos;
}
