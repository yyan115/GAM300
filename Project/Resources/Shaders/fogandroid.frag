#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;

in vec3 FragPos;
out vec4 FragColor;

// ============================================================================
// Fog Uniforms
// ============================================================================
uniform int fogShape; // 0 = BOX, 1 = SPHERE, 2 = CYLINDER

uniform vec3  fogColor;
uniform float density;
uniform float opacity;

uniform float time;
uniform float scrollSpeedX;
uniform float scrollSpeedY;
uniform float noiseScale;
uniform float noiseStrength;

uniform bool  useHeightFade;
uniform float heightFadeStart;
uniform float heightFadeEnd;

uniform float edgeSoftness;

uniform sampler2D noiseMap;
uniform bool hasNoiseMap;

uniform vec3 cameraPos;
uniform mat4 modelInverse;

// ============================================================================
// Procedural Noise Functions
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
    f = f * f * (3.0 - 2.0 * f);

    return mix(
        mix(mix(hash(i + vec3(0.0, 0.0, 0.0)), hash(i + vec3(1.0, 0.0, 0.0)), f.x),
            mix(hash(i + vec3(0.0, 1.0, 0.0)), hash(i + vec3(1.0, 1.0, 0.0)), f.x), f.y),
        mix(mix(hash(i + vec3(0.0, 0.0, 1.0)), hash(i + vec3(1.0, 0.0, 1.0)), f.x),
            mix(hash(i + vec3(0.0, 1.0, 1.0)), hash(i + vec3(1.0, 1.0, 1.0)), f.x), f.y),
        f.z
    );
}

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
    vec3 rayDir = normalize(FragPos - cameraPos);
    vec3 localCamPos = (modelInverse * vec4(cameraPos, 1.0)).xyz;
    
    // Safer matrix cast for mobile GPUs
    mat3 rotMat = mat3(
        modelInverse[0].xyz,
        modelInverse[1].xyz,
        modelInverse[2].xyz
    );
    vec3 localRayDir = normalize(rotMat * rayDir);

    float tEntry = -1.0;
    float tExit  = -1.0;

    // --- Ray-Shape Intersection ---
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
        float b = dot(localCamPos, localRayDir);
        float c = dot(localCamPos, localCamPos) - 0.25; 
        float d = b*b - c; 
        
        if (d >= 0.0) 
        {
            float sqrtD = sqrt(d);
            tEntry = -b - sqrtD;
            tExit  = -b + sqrtD;
        }
    }
    else if (fogShape == 2) // CYLINDER
    {
        float a = dot(localRayDir.xz, localRayDir.xz);
        float b = dot(localCamPos.xz, localRayDir.xz);
        float c = dot(localCamPos.xz, localCamPos.xz) - 0.25;
        float d = b*b - a*c;

        if (d >= 0.0 && a > 0.00001) 
        {
            float sqrtD = sqrt(d);
            float t0_cyl = (-b - sqrtD) / a;
            float t1_cyl = (-b + sqrtD) / a;

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
                d = -1.0; 
            }

            if (d >= 0.0) 
            {
                tEntry = max(t0_cyl, t_yMin);
                tExit = min(t1_cyl, t_yMax);
            }
        }
    }

    tEntry = max(tEntry, 0.0);
    if (tExit <= tEntry) discard; 
    
    float thickness = tExit - tEntry;

    vec3 sampleLocal = localCamPos + (tEntry + thickness * 0.5) * localRayDir;
    vec3 samplePos   = clamp(sampleLocal + 0.5, 0.0, 1.0);

    // --- Noise Sampling ---
    vec3 noiseCoord = samplePos * noiseScale + vec3(
        time * scrollSpeedX,
        time * scrollSpeedY,
        time * scrollSpeedX * 0.5
    );

    float noiseValue;
    if (hasNoiseMap)
    {
        vec2 noiseUV = samplePos.xz * noiseScale + vec2(time * scrollSpeedX, time * scrollSpeedY);
        noiseValue = texture(noiseMap, noiseUV).r;
    }
    else
    {
        noiseValue = fbm(noiseCoord);
    }

    float fogDensity = density * thickness * mix(1.0, noiseValue, noiseStrength);

    // --- Height Fade ---
    if (useHeightFade)
    {
        float heightFactor = smoothstep(heightFadeEnd, heightFadeStart, samplePos.y);
        fogDensity *= heightFactor;
    }

    // --- Edge Softness based on shape ---
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
            float radialDist = 0.5 - length(sampleLocal.xz);
            float verticalDist = 0.5 - abs(sampleLocal.y);
            edgeDist = min(radialDist, verticalDist);
        }
        
        float edgeFade = smoothstep(0.0, edgeSoftness * 0.5, edgeDist);
        fogDensity *= edgeFade;
    }

    float finalAlpha = 1.0 - exp(-fogDensity);
    finalAlpha = clamp(finalAlpha * opacity, 0.0, opacity);

    if (finalAlpha < 0.001)
    {
        discard;
    }

    FragColor = vec4(fogColor, finalAlpha);
}