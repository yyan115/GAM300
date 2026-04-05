#version 300 es
precision highp float;
precision highp sampler2D;

out vec4 FragColor;

in vec3 TexCoords;

uniform sampler2D skyboxTexture;

void main()
{
    vec3 dir = normalize(TexCoords);

    float u = 0.5 + atan(dir.z, dir.x) / (2.0 * 3.14159265359);
    float v = 0.5 - asin(dir.y) / 3.14159265359;

    vec4 texColor = texture(skyboxTexture, vec2(u, v));
    FragColor = texColor;
}
