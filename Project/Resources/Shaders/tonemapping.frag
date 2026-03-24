#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D hdrBuffer;
uniform float exposure;
uniform float gamma;
uniform int toneMappingMode;
uniform bool enableTonemapping;  // If false, bypass tonemapping

// Vignette
uniform bool vignetteEnabled;
uniform float vignetteIntensity;
uniform float vignetteSmoothness;
uniform vec3 vignetteColor;

// Color Grading
uniform bool colorGradingEnabled;
uniform float cgBrightness;
uniform float cgContrast;
uniform float cgSaturation;
uniform vec3 cgTint;

// Chromatic Aberration
uniform bool caEnabled;
uniform float caIntensity;
uniform float caPadding;

// SSAO
uniform sampler2D ssaoTexture;
uniform bool ssaoEnabled;

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
    vec3 hdrColor;
    if (caEnabled && caIntensity > 0.0)
    {
        vec2 uv = TexCoords - 0.5;
        float dist = length(uv);
        // Padding controls the falloff: higher padding = effect only at edges
        float edgeFactor = smoothstep(caPadding * 0.5, 1.0, dist * 2.0);
        float offset = caIntensity * 0.01 * edgeFactor;

        // Sample each channel with offset toward/away from center
        vec2 dir = normalize(uv + 0.0001);
        hdrColor.r = texture(hdrBuffer, TexCoords + dir * offset).r;
        hdrColor.g = texture(hdrBuffer, TexCoords).g;
        hdrColor.b = texture(hdrBuffer, TexCoords - dir * offset).b;
    }
    else
    {
        hdrColor = texture(hdrBuffer, TexCoords).rgb;
    }
    
    // Apply SSAO
    if (ssaoEnabled) {
        float ao = texture(ssaoTexture, TexCoords).r;
        hdrColor *= ao;
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
            mapped = ExposureToneMapping(hdrColor, exposure);
        }
        else if (toneMappingMode == 2)
        {
            mapped = ACESFilm(mapped);
        }
    }

    // Vignette: blend toward vignetteColor at edges
    if (vignetteEnabled)
    {
        vec2 uv = TexCoords - 0.5;
        float vignette = 1.0 - dot(uv, uv) * vignetteIntensity * 4.0;
        vignette = smoothstep(0.0, vignetteSmoothness + 0.5, vignette);
        mapped = mix(vignetteColor, mapped, vignette);
    }

    // Color Grading
    if (colorGradingEnabled)
    {
        mapped += cgBrightness;
        mapped = (mapped - 0.5) * cgContrast + 0.5;
        float luma = dot(mapped, vec3(0.2126, 0.7152, 0.0722));
        mapped = mix(vec3(luma), mapped, cgSaturation);
        mapped *= cgTint;
    }

    mapped = clamp(mapped, 0.0, 1.0);
    FragColor = vec4(mapped, 1.0);
}