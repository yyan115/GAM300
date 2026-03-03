#version 300 es
precision highp float;

layout (location = 0) in vec3 aPos;

out vec3 FragPos;
out vec3 LocalPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    // Convert from [-0.5, 0.5] to [0, 1] for clean sampling in frag shader
    LocalPos = aPos + 0.5;
    gl_Position = projection * view * worldPos;
}