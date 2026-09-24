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
uniform bool noiseCacheBuild;

void main()
{
    if (noiseCacheBuild) {
        vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
        gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
        FragPos = vec3(0.0);
        LocalPos = vec3(0.0);
        return;
    }
    vec4 worldPos = model * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    // Convert from [-0.5, 0.5] to [0, 1] for clean sampling in frag shader
    LocalPos = aPos + 0.5;

    gl_Position = projection * view * worldPos;
}
