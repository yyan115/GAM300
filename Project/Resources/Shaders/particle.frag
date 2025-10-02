#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in vec4 ParticleColor;

uniform sampler2D particleTexture;

void main() 
{
    vec4 texColor = texture(particleTexture, TexCoord);
    FragColor = texColor * ParticleColor;
}