#version 330 core

in vec3 FragPos;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BloomEmission; // fog does not emit bloom

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
uniform float warpStrength;   // Domain warp intensity (0 = off, 1+ = smoky swirls)

// Height fade
uniform bool  useHeightFade;
uniform float heightFadeStart;
uniform float heightFadeEnd;

// Edge softness
uniform float edgeSoftness;

// Optional noise texture
uniform sampler2D noiseMap;
uniform bool hasNoiseMap;
uniform int  noiseTextureMappingAxis;  // 0=XZ (top-down), 1=XY (front), 2=YZ (side)

// Optional color/material texture — tints fog with designer-authored image
uniform sampler2D colorMap;
uniform bool      hasColorMap;
uniform float     colorTextureIntensity;
uniform float     colorTextureScale;

// Camera UBO (binding = 0)
layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
};

// World-to-local transform for ray-box intersection
uniform mat4 modelInverse;

uniform int fogShape;

// Depth-based soft intersection with solid geometry
uniform sampler2D depthTexture;
uniform vec2      viewportSize;
uniform mat4      inverseProjection;
uniform mat4      inverseView;
uniform float     nearPlane;
uniform float     farPlane;

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
// 3 octaves instead of 4: ~25% cheaper with negligible visual difference
float fbm(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 3; i++)
    {
        value += amplitude * valueNoise(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}

// Cheaper 2-octave fbm used only for domain warp — retains visible motion
// at roughly half the cost of the full fbm
float fbmWarp(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 2; i++)
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
    float tEntry = -1.0;
    float tExit  = -1.0;

    if (fogShape == 0) // BOX
    {
        vec3 invDir = 1.0 / localRayDir;
        vec3 t0 = (vec3(-0.5) - localCamPos) * invDir;
        vec3 t1 = (vec3( 0.5) - localCamPos) * invDir;
        vec3 tMinV = min(t0, t1);
        vec3 tMaxV = max(t0, t1);
        tEntry = max(max(tMinV.x, tMinV.y), tMinV.z);
        tExit  = min(min(tMaxV.x, tMaxV.y), tMaxV.z);
    }
    else if (fogShape == 1) // SPHERE
    {
        // Sphere formula: localCamPos + t * localRayDir = radius (0.5)
        float b = dot(localCamPos, localRayDir);
        float c = dot(localCamPos, localCamPos) - 0.25; // 0.25 is r^2
        float d = b*b - c; 
        
        if (d >= 0.0) 
        {
            float sqrtD = sqrt(d);
            tEntry = -b - sqrtD;
            tExit  = -b + sqrtD;
        }
    }
    else if (fogShape == 2) // CYLINDER (Aligned to Y axis)
    {
        // Check XZ plane for radial intersection
        float a = dot(localRayDir.xz, localRayDir.xz);
        float b = dot(localCamPos.xz, localRayDir.xz);
        float c = dot(localCamPos.xz, localCamPos.xz) - 0.25;
        float d = b*b - a*c;

        if (d >= 0.0 && a > 0.00001) 
        {
            float sqrtD = sqrt(d);
            float t0_cyl = (-b - sqrtD) / a;
            float t1_cyl = (-b + sqrtD) / a;

            // Check Y axis for top/bottom caps limits
            float t_yMin = -99999.0;
            float t_yMax =  99999.0;
            
            if (abs(localRayDir.y) > 0.00001) 
            {
                float t_cap0 = (-0.5 - localCamPos.y) / localRayDir.y;
                float t_cap1 = (0.5 - localCamPos.y) / localRayDir.y;
                t_yMin = min(t_cap0, t_cap1);
                t_yMax = max(t_cap0, t_cap1);
            } 
            else if (abs(localCamPos.y) > 0.5) 
            {
                d = -1.0; // Ray is parallel to caps and outside Y bounds (miss)
            }

            if (d >= 0.0) 
            {
                tEntry = max(t0_cyl, t_yMin);
                tExit = min(t1_cyl, t_yMax);
            }
        }
    }
    tEntry = max(tEntry, 0.0);   // camera inside volume: start from camera

    if (tExit <= tEntry) discard;

    // --- 3. Depth-based solid geometry intersection ---
    // Reconstruct world position of solid geometry from the scene depth buffer.
    // Cap tExit so the fog path stops at any solid surface inside the volume.
    // Beer-Lambert then naturally produces a smooth exponential fade at intersections.
    {
        vec2 screenUV  = gl_FragCoord.xy / viewportSize;
        float sceneNDC = texture(depthTexture, screenUV).r;

        // Only cap when there is real geometry (depth < 1.0 = far plane / sky)
        if (sceneNDC < 1.0)
        {
            // Unproject depth-buffer value to world space
            vec4 ndcPos   = vec4(screenUV * 2.0 - 1.0, sceneNDC * 2.0 - 1.0, 1.0);
            vec4 viewPos  = inverseProjection * ndcPos;
            viewPos      /= viewPos.w;
            vec3 solidWorld = (inverseView * viewPos).xyz;

            // Convert to local fog space, project onto local ray → t_solid
            vec3  solidLocal = (modelInverse * vec4(solidWorld, 1.0)).xyz;
            float t_solid    = dot(solidLocal - localCamPos, localRayDir);

            tExit = min(tExit, t_solid);
        }
    }

    if (tExit <= tEntry) discard;

    float thickness = tExit - tEntry;

    // --- 3. Ray march through the volume ---
    // Multiple samples along the ray accumulate density, creating visible internal
    // structure (wisps, patches) that actually animate when noise is scrolled/warped.
    const int MARCH_STEPS = 8;
    float stepSize    = thickness / float(MARCH_STEPS);
    float transmittance = 1.0;

    for (int i = 0; i < MARCH_STEPS; i++)
    {
        float t = tEntry + (float(i) + 0.5) * stepSize;
        vec3 sampleLocal = localCamPos + t * localRayDir;
        vec3 samplePos   = clamp(sampleLocal + 0.5, 0.0, 1.0);

        // Noise coordinate with time scroll
        vec3 noiseCoord = samplePos * noiseScale + vec3(
            time * scrollSpeedX,
            time * scrollSpeedY,
            time * scrollSpeedX * 0.5
        );

        // Domain warping: distort the noise coordinate for swirling smoke effect.
        // Uses 2-octave fbmWarp instead of full 4-octave fbm — half the cost, motion still visible.
        if (warpStrength > 0.0)
        {
            vec3 warp = vec3(
                fbmWarp(noiseCoord + vec3(1.7, 9.2, 3.4)),
                fbmWarp(noiseCoord + vec3(8.3, 2.8, 5.1)),
                fbmWarp(noiseCoord + vec3(4.5, 6.1, 1.9))
            );
            noiseCoord += warpStrength * warp;
        }

        float noiseValue;
        if (hasNoiseMap)
        {
            vec2 baseUV;
            if (noiseTextureMappingAxis == 1)       // XY — front-facing (good for rising smoke)
                baseUV = samplePos.xy;
            else if (noiseTextureMappingAxis == 2)  // YZ — side-facing
                baseUV = samplePos.yz;
            else                                    // XZ — top-down (default)
                baseUV = samplePos.xz;
            vec2 noiseUV = baseUV * noiseScale + vec2(time * scrollSpeedX, time * scrollSpeedY);
            noiseValue = texture(noiseMap, noiseUV).r;
        }
        else
        {
            noiseValue = fbm(noiseCoord);
        }

        // FBM returns ~[0.3, 0.7] - remap to [0, 1] so low-noise areas become truly
        // transparent and high-noise areas become dense, creating visible wisps.
        float shapedNoise = smoothstep(0.3, 0.7, noiseValue);
        float stepDensity = density * stepSize * mix(1.0, shapedNoise, noiseStrength);

        // Height fade
        if (useHeightFade)
        {
            float heightFactor = smoothstep(heightFadeEnd, heightFadeStart, samplePos.y);
            stepDensity *= heightFactor;
        }

        // Edge softness
        if (edgeSoftness > 0.0)
        {
            float edgeDist = 0.0;
            if (fogShape == 0) // BOX
            {
                float edgeX = min(samplePos.x, 1.0 - samplePos.x);
                float edgeY = min(samplePos.y, 1.0 - samplePos.y);
                float edgeZ = min(samplePos.z, 1.0 - samplePos.z);
                edgeDist = min(edgeX, min(edgeY, edgeZ));
            }
            else if (fogShape == 1) // SPHERE
            {
                edgeDist = 0.5 - length(sampleLocal);
            }
            else if (fogShape == 2) // CYLINDER
            {
                float radialDist  = 0.5 - length(sampleLocal.xz);
                float verticalDist = 0.5 - abs(sampleLocal.y);
                edgeDist = min(radialDist, verticalDist);
            }
            float edgeFade = smoothstep(0.0, edgeSoftness * 0.5, edgeDist);
            stepDensity *= edgeFade;
        }

        transmittance *= exp(-stepDensity);
        if (transmittance < 0.01) break; // early exit once fully opaque
    }

    // --- 4. Final output ---
    float finalAlpha = clamp((1.0 - transmittance) * opacity, 0.0, opacity);

    if (finalAlpha < 0.001)
    {
        discard;
    }

    // Color texture: sample at ray midpoint in local XZ space, tint fogColor
    vec3 finalColor = fogColor;
    if (hasColorMap)
    {
        float tMid = (tEntry + tExit) * 0.5;
        vec3  midLocal = localCamPos + tMid * localRayDir;
        vec2  colorUV  = (clamp(midLocal.xz + 0.5, 0.0, 1.0)) * colorTextureScale;
        vec3  texTint  = texture(colorMap, colorUV).rgb;
        finalColor = fogColor * mix(vec3(1.0), texTint, colorTextureIntensity);
    }

    FragColor = vec4(finalColor, finalAlpha);
    BloomEmission = vec4(0.0);
}
