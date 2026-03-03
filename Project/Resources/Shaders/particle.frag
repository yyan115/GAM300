#version 330 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 BloomEmission;

in vec2 TexCoord;
in vec4 ParticleColor;

uniform sampler2D particleTexture;

// Per-entity bloom emission
uniform float bloomIntensity;
uniform vec3 bloomColor;

void main() 
{
    vec4 texColor = texture(particleTexture, TexCoord);
    FragColor = texColor * ParticleColor;

    // Per-entity bloom emission — written only to MRT attachment 1
    BloomEmission = vec4(bloomColor * bloomIntensity, 1.0);
}