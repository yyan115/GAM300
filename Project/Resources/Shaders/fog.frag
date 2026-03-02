#version 330 core

in vec3 FragPos;
in vec3 LocalPos;       // [0, 1] within the volume

out vec4 FragColor;

// ============================================================================
// Fog Uniforms (mapped 1:1 from FogVolumeComponent)
// ============================================================================
uniform vec3  fogColor;
uniform float density;
uniform float opacity;

// Noise animation
uniform float time;
uniform float scrollSpeedX;
uniform float scrollSpeedY;
uniform float noiseScale;
uniform float noiseStrength;

// Height fade
uniform bool  useHeightFade;
uniform float heightFadeStart;
uniform float heightFadeEnd;

// Edge softness
uniform float edgeSoftness;

// Optional noise texture
uniform sampler2D noiseMap;
uniform bool hasNoiseMap;

// Camera
uniform vec3 cameraPos;

// ============================================================================
// Procedural Noise
// Based on value noise with FBM layering for natural fog appearance
// ============================================================================

float hash(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep

    return mix(
        mix(mix(hash(i + vec3(0,0,0)), hash(i + vec3(1,0,0)), f.x),
            mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
        mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
            mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y),
        f.z
    );
}

// Fractional Brownian Motion - layered noise for natural wisps
float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 4; i++)
    {
        value += amplitude * valueNoise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    // --- 1. Base density ---
    float fogDensity = density;

    // --- 2. Noise sampling for wispy / smoky look ---
    vec3 noiseCoord = LocalPos * noiseScale + vec3(
        time * scrollSpeedX,
        time * scrollSpeedY,
        time * scrollSpeedX * 0.5
    );

    float noiseValue;
    if (hasNoiseMap)
    {
        // Sample provided noise texture on XZ plane
        vec2 noiseUV = LocalPos.xz * noiseScale + vec2(time * scrollSpeedX, time * scrollSpeedY);
        noiseValue = texture(noiseMap, noiseUV).r;
    }
    else
    {
        // Procedural FBM
        noiseValue = fbm(noiseCoord);
    }

    // Blend between uniform density and noisy density
    fogDensity *= mix(1.0, noiseValue, noiseStrength);

    // --- 3. Height fade (local Y: 0 = bottom, 1 = top) ---
    if (useHeightFade)
    {
        // smoothstep fades from 1.0 at heightFadeStart to 0.0 at heightFadeEnd
        float heightFactor = smoothstep(heightFadeEnd, heightFadeStart, LocalPos.y);
        fogDensity *= heightFactor;
    }

    // --- 4. Edge softness ---
    if (edgeSoftness > 0.0)
    {
        // Distance from nearest edge in each axis (0 at edge, 0.5 at center)
        float edgeX = min(LocalPos.x, 1.0 - LocalPos.x);
        float edgeY = min(LocalPos.y, 1.0 - LocalPos.y);
        float edgeZ = min(LocalPos.z, 1.0 - LocalPos.z);
        float edgeDist = min(min(edgeX, edgeY), edgeZ);

        float edgeFade = smoothstep(0.0, edgeSoftness * 0.5, edgeDist);
        fogDensity *= edgeFade;
    }

    // --- 5. Distance fade (subtle, avoids popping at far range) ---
    float distToCamera = length(FragPos - cameraPos);
    float distFade = 1.0 - smoothstep(50.0, 100.0, distToCamera);
    fogDensity *= distFade;

    // --- 6. Final output ---
    float finalAlpha = clamp(fogDensity * opacity, 0.0, opacity);

    if (finalAlpha < 0.001)
    {
        discard;
    }

    FragColor = vec4(fogColor, finalAlpha);
}
