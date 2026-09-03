#version 300 es
precision highp float;

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aInstancePos;
layout (location = 3) in float aInstanceLife;
layout (location = 4) in vec2 aInstanceRotationSinCos;

out vec2 TexCoord;
out vec4 ParticleColor;

layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
    mat4 viewProjection;
};

uniform vec3 cameraRight;
uniform vec3 cameraUp;
uniform vec4 particleStartColor;
uniform vec4 particleEndColor;
uniform float particleStartSize;
uniform float particleEndSize;

void main() 
{
    TexCoord = aTexCoord;
    float age = 1.0 - clamp(aInstanceLife, 0.0, 1.0);
    ParticleColor = mix(particleStartColor, particleEndColor, age);
    float particleSize = mix(particleStartSize, particleEndSize, age);
    
    float s = aInstanceRotationSinCos.x;
    float c = aInstanceRotationSinCos.y;
    vec2 rotatedPos = vec2(
        aPos.x * c - aPos.y * s,
        aPos.x * s + aPos.y * c
    );
    
    // Billboard the particle
    vec3 worldPos = aInstancePos 
        + cameraRight * rotatedPos.x * particleSize
        + cameraUp * rotatedPos.y * particleSize;
    
    gl_Position = viewProjection * vec4(worldPos, 1.0);
}
