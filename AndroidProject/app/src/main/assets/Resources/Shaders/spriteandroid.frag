#version 300 es
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec4 spriteColor;

void main()
{
    vec4 texColor = texture(spriteTexture, TexCoord);
    FragColor = texColor * spriteColor;
}