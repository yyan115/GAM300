#version 330 core
layout (location = 0) in vec2 aPos;       // Quad vertex position
layout (location = 1) in vec2 aTexCoord;  // Quad UV
layout (location = 2) in vec3 aInstancePos;     // Per-particle position
layout (location = 3) in vec4 aInstanceColor;   // Per-particle color
layout (location = 4) in float aInstanceSize;   // Per-particle size
layout (location = 5) in float aInstanceRotation; // Per-particle rotation

out vec2 TexCoord;
out vec4 ParticleColor;

uniform mat4 view;
uniform mat4 projection;
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