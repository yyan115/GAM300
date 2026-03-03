#version 330 core
in vec2 TexCoords;
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 BloomEmission;

uniform sampler2D text;
uniform vec4 textColor;

// Per-entity bloom emission
uniform float bloomIntensity;
uniform vec3 bloomColor;

void main()
{
    float a = texture(text, TexCoords).r;
    color = vec4(textColor.rgb, textColor.a * a);

    // Per-entity bloom emission — written only to MRT attachment 1
    BloomEmission = vec4(bloomColor * bloomIntensity, 1.0);
}
