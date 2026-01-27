#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec4 spriteColor;

void main()
{
    vec4 texColor = texture(spriteTexture, TexCoord);
    
    // Apply color tinting and alpha
    FragColor = texColor * spriteColor;

    // Discard transparent pixels to avoid rendering them
    if (FragColor.a < 0.01)
        discard;
}