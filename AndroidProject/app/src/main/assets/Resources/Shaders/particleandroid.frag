#version 300 es
precision mediump float;

in vec2 TexCoord;
in vec4 ParticleColor;

out vec4 FragColor;

uniform sampler2D particleTexture;

void main() 
{
    vec4 texColor = texture(particleTexture, TexCoord);
    FragColor = texColor * ParticleColor;
}