#version 300 es
precision mediump float;
precision highp sampler2D;

out vec4 FragColor;
in highp vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform sampler2D bloomTexture;
uniform float bloomIntensity;
uniform float exposure;
uniform float inverseGamma;
uniform int toneMappingMode;
uniform bool enableTonemapping;  // If false, bypass tonemapping

vec3 ReinhardToneMapping(vec3 color)
{
    return color / (color + vec3(1.0));
}

vec3 ExposureToneMapping(vec3 color, float exposureVal)
{
    return vec3(1.0) - exp(-color * exposureVal);
}

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    // The shared final clamp below immediately clamps every tone-map mode.
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main()
{
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;
    if (bloomIntensity > 0.0)
    {
        hdrColor += texture(bloomTexture, TexCoords).rgb * bloomIntensity;
    }
    
    // Apply exposure adjustment
    vec3 mapped = hdrColor * exposure;
    
    if (enableTonemapping)
    {
        if (toneMappingMode == 0)
        {
            mapped = ReinhardToneMapping(mapped);
        }
        else if (toneMappingMode == 1)
        {
            mapped = ExposureToneMapping(mapped, 1.0);
        }
        else if (toneMappingMode == 2)
        {
            mapped = ACESFilm(mapped);
        }
    }

    mapped = clamp(mapped, 0.0, 1.0);
    // Apply gamma correction
    mapped = pow(mapped, vec3(inverseGamma));
    FragColor = vec4(mapped, 1.0);
}
