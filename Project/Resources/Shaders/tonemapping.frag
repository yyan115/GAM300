#version 330 core

out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform float exposure;
uniform float gamma;
uniform int toneMappingMode; // 0=Reinhard, 1=Exposure, 2=ACES

// Reinhard tone mapping
vec3 ReinhardToneMapping(vec3 color)
{
    return color / (color + vec3(1.0));
}

// Exposure tone mapping
vec3 ExposureToneMapping(vec3 color, float exposureVal)
{
    return vec3(1.0) - exp(-color * exposureVal);
}

// ACES Filmic tone mapping (cinematic)
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
    // Sample HDR color
    vec3 hdrColor = texture(hdrBuffer, TexCoords).rgb;
    
    // Apply exposure adjustment
    vec3 mapped = hdrColor * exposure;
    
    // Apply tone mapping based on mode
    if (toneMappingMode == 0) {
        // Reinhard
        mapped = ReinhardToneMapping(mapped);
    } else if (toneMappingMode == 1) {
        // Exposure-based
        mapped = ExposureToneMapping(mapped, 1.0);
    } else if (toneMappingMode == 2) {
        // ACES Filmic
        mapped = ACESFilm(mapped);
    }
    
    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / gamma));
    
    FragColor = vec4(mapped, 1.0);
}