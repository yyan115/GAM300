#version 300 es
precision highp float;
precision highp sampler2D;

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform float exposure;
uniform float gamma;
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
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;
    
    // Apply exposure adjustment
    vec3 mapped = hdrColor * exposure;
    
    // Only apply tone mapping if enabled AND we have HDR content
    float maxComponent = max(max(mapped.r, mapped.g), mapped.b);

    if (enableTonemapping && maxComponent > 1.0)
    {
        // Apply tone mapping only to bright areas
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
    // else: values pass through unchanged (will clip at 1.0 in output framebuffer)

    FragColor = vec4(mapped, 1.0);
}
