#version 300 es
precision highp float;

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aInstancePos;
layout (location = 3) in vec4 aInstanceColor;
layout (location = 4) in float aInstanceSize;
layout (location = 5) in float aInstanceRotation;

out vec2 TexCoord;
out vec4 ParticleColor;

layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
};

uniform vec3 cameraRight;
uniform vec3 cameraUp;

void main() 
{
    TexCoord = aTexCoord;
    ParticleColor = aInstanceColor;
    
    // Calculate rotation
    float c = cos(aInstanceRotation);
    float s = sin(aInstanceRotation);
    vec2 rotatedPos = vec2(
        aPos.x * c - aPos.y * s,
        aPos.x * s + aPos.y * c
    );
    
    // Billboard the particle
    vec3 worldPos = aInstancePos 
        + cameraRight * rotatedPos.x * aInstanceSize
        + cameraUp * rotatedPos.y * aInstanceSize;
    
    gl_Position = projection * view * vec4(worldPos, 1.0);
}