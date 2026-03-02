#version 330 core

in vec3 FragPos;

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

// World-to-local transform for ray-box intersection
uniform mat4 modelInverse;

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
// Main - volumetric ray-box intersection
// ============================================================================

void main()
{
    // --- 1. Ray in world space ---
    vec3 rayDir = normalize(FragPos - cameraPos);

    // Transform ray into local box space [-0.5, 0.5]
    vec3 localCamPos = (modelInverse * vec4(cameraPos, 1.0)).xyz;
    vec3 localRayDir = normalize(mat3(modelInverse) * rayDir);

    // --- 2. Ray-AABB intersection (slab method) ---
    vec3 invDir = 1.0 / localRayDir;
    vec3 t0 = (vec3(-0.5) - localCamPos) * invDir;
    vec3 t1 = (vec3( 0.5) - localCamPos) * invDir;
    vec3 tMinV = min(t0, t1);
    vec3 tMaxV = max(t0, t1);
    float tEntry = max(max(tMinV.x, tMinV.y), tMinV.z);
    float tExit  = min(min(tMaxV.x, tMaxV.y), tMaxV.z);
    tEntry = max(tEntry, 0.0);   // camera inside volume: start from camera

    if (tExit <= tEntry) discard;

    float thickness = tExit - tEntry;

    // Sample the volume at the midpoint of the ray path (in [0,1] space)
    vec3 sampleLocal = localCamPos + (tEntry + thickness * 0.5) * localRayDir;
    vec3 samplePos   = clamp(sampleLocal + 0.5, 0.0, 1.0);

    // --- 3. Noise sampling for wispy / smoky look ---
    vec3 noiseCoord = samplePos * noiseScale + vec3(
        time * scrollSpeedX,
        time * scrollSpeedY,
        time * scrollSpeedX * 0.5
    );

    float noiseValue;
    if (hasNoiseMap)
    {
        // Sample provided noise texture on XZ plane
        vec2 noiseUV = samplePos.xz * noiseScale + vec2(time * scrollSpeedX, time * scrollSpeedY);
        noiseValue = texture(noiseMap, noiseUV).r;
    }
    else
    {
        // Procedural FBM
        noiseValue = fbm(noiseCoord);
    }

    // Beer-Lambert: accumulate density over path length through volume
    float fogDensity = density * thickness * mix(1.0, noiseValue, noiseStrength);

    // --- 4. Height fade (sample Y: 0 = bottom, 1 = top) ---
    if (useHeightFade)
    {
        float heightFactor = smoothstep(heightFadeEnd, heightFadeStart, samplePos.y);
        fogDensity *= heightFactor;
    }

    // --- 5. Edge softness (samplePos is in the volume interior, not on a face) ---
    if (edgeSoftness > 0.0)
    {
        float edgeX = min(samplePos.x, 1.0 - samplePos.x);
        float edgeY = min(samplePos.y, 1.0 - samplePos.y);
        float edgeZ = min(samplePos.z, 1.0 - samplePos.z);
        float edgeDist = min(edgeX, min(edgeY, edgeZ));
        float edgeFade = smoothstep(0.0, edgeSoftness * 0.5, edgeDist);
        fogDensity *= edgeFade;
    }

    // --- 6. Final output ---
    float finalAlpha = 1.0 - exp(-fogDensity);
    finalAlpha = clamp(finalAlpha * opacity, 0.0, opacity);

    if (finalAlpha < 0.001)
    {
        discard;
    }

    FragColor = vec4(fogColor, finalAlpha);
}
