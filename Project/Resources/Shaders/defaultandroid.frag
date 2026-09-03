#version 300 es
precision mediump float;
precision highp int;
precision highp sampler2D;

// ============================================================================
// Material
// OpenGL ES requires samplers outside of structs
// ============================================================================
layout(std140) uniform MaterialBlock {
    vec4 u_materialDiffuseOpacity;
    vec4 u_materialEmissiveMetallic;
    vec4 u_materialRoughnessTermsAO;
    vec4 u_materialUVTransform;
    uvec4 u_materialFeatures;
};

const uint MATERIAL_FEATURE_DIFFUSE = 1u << 0;
const uint MATERIAL_FEATURE_NORMAL = 1u << 1;
const uint MATERIAL_FEATURE_EMISSIVE = 1u << 2;
const uint MATERIAL_FEATURE_AO = 1u << 3;
const uint MATERIAL_FEATURE_METALLIC = 1u << 4;
const uint MATERIAL_FEATURE_ROUGHNESS = 1u << 5;
const uint MATERIAL_FEATURE_OPACITY = 1u << 6;
const uint MATERIAL_FEATURE_PACKED_ORM = 1u << 7;
const uint MATERIAL_FEATURE_DIELECTRIC = 1u << 8;

// Samplers must be declared outside structs in OpenGL ES
uniform sampler2D diffuseMap;
uniform sampler2D normalMap;
uniform sampler2D emissiveMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D opacityMap;
uniform sampler2D packedORMMap;

in mediump vec2 TexCoords;
in highp vec3 FragPos;
in mediump vec3 Normal;
in mediump vec3 Tangent;
flat in mediump vec4 vBloomData;
flat in uint vLightMask;

// ============================================================================
// Lighting Structures (packed for std140 UBO)
// All lighting data lives in the LightingBlock UBO (binding = 1) — uploaded
// once per frame by LightingSystem instead of via per-draw uniform setters.
// Fields are packed into vec4s to keep std140 layout deterministic and avoid
// vec3-padding footguns.
// ============================================================================
#define NR_POINT_LIGHTS 8
#define NR_SPOT_LIGHTS 4

struct PointLightGPU {
    highp vec4 positionInvRange; // xyz = position, w = 1/range (0 = unlimited)
    vec4 diffuseLinear;        // xyz = diffuse,  w = linear
    vec4 attenuationIntensity; // x = constant, y = quadratic, z = intensity
};

struct SpotLightGPU {
    highp vec4 positionCutoff; // xyz = position,  w = cutOff
    vec4 directionOuter;       // xyz = direction, w = outerCutOff
    vec4 diffuseLinear;        // xyz = diffuse,   w = linear
    vec4 attenuationIntensity; // xyz = constant/quadratic/intensity,
                               // w = 1/(cutOff-outerCutOff)
};

// Lighting UBO (binding = 1)
layout(std140) uniform LightingBlock {
    vec4 u_ambSkyIntensity;      // xyz = ambientSky,     w = ambientIntensity
    vec4 u_ambEquatorMode;       // xyz = ambientEquator, w = pad
    vec4 u_ambGround;            // xyz = ambientGround,  w = pad
    vec4 u_dirLightDir;          // xyz = direction,      w = intensity
    vec4 u_dirLightDiffuse;      // xyz = diffuse,        w = hasDirectionalLight
    ivec4 u_lightCounts;         // xy = counts, z = active mask,
                                 // w = ambient mode (-1 when disabled)
    PointLightGPU u_pointLights[NR_POINT_LIGHTS];
    SpotLightGPU u_spotLights[NR_SPOT_LIGHTS];
};

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BloomEmission;

// Camera UBO (binding = 0)
layout(std140) uniform CameraBlock {
    highp mat4 view;
    highp mat4 projection;
    highp vec3 cameraPos;
    highp float _pad;
    highp mat4 viewProjection;
};

// Distance-based fade opacity (0 = invisible, 1 = fully visible)
uniform float u_distanceFadeOpacity;

// Per-entity bloom emission
uniform bool useInstancing;
uniform float bloomIntensity;
uniform vec3 bloomColor;

// Per-entity brightness multiplier (e.g. player = 1.35)
uniform float brightnessBoost;

// Point lights use bits 0-7 and spot lights use bits 8-11. Animated/manual
// models that cannot provide conservative bounds use all bits.
uniform int u_lightMask;

// ============================================================================
// Helper functions for materials
// ============================================================================

vec3 calculateAmbient(vec3 normal) {
    int mode = u_lightCounts.w;
    vec3 ambientSky = u_ambSkyIntensity.xyz;
    vec3 ambientEquator = u_ambEquatorMode.xyz;
    vec3 ambientGround = u_ambGround.xyz;
    vec3 ambient;

    if (mode == 0) {
        ambient = ambientSky;
    } else if (mode == 1) {
        float t = normal.y * 0.5 + 0.5;
        if (t < 0.5) {
            ambient = mix(ambientGround, ambientEquator, t * 2.0);
        } else {
            ambient = mix(ambientEquator, ambientSky, (t - 0.5) * 2.0);
        }
    } else {
        ambient = ambientSky;
    }

    return ambient * u_ambSkyIntensity.w;  // w = ambientIntensity
}

vec3 getNormalFromMap(vec2 uv, bool hasNormalMap) {
    if (hasNormalMap) {
        vec2 enc = texture(normalMap, uv).rg * 2.0 - 1.0;
        float z = sqrt(max(0.0, 1.0 - dot(enc, enc)));
        vec3 tangentNormal = normalize(vec3(enc, z));

        vec3 N = normalize(Normal);
        vec3 T = normalize(Tangent);
        vec3 B = cross(N, T);
        mat3 TBN = mat3(T, B, N);

        return normalize(TBN * tangentNormal);
    }
    return normalize(Normal);
}

// ============================================================================
// PBR Helper Functions (Cook-Torrance BRDF)
// ============================================================================
const float PI = 3.14159265359;

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    float x = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x2 = x * x;
    return F0 + (1.0 - F0) * x2 * x2 * x;
}

vec3 evaluatePBR(
    vec3 N, vec3 V, vec3 L, float NdotL,
    vec3 diffuseBase, vec3 F0,
    float NdotV, float roughness4, float geometryK,
    float geometryViewDenom)
{
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float distributionDenom =
        NdotH * NdotH * (roughness4 - 1.0) + 1.0;
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    // Combine GGX distribution, both Smith geometry terms, and the
    // Cook-Torrance denominator. This is algebraically equivalent but needs
    // one reciprocal instead of the original four.
    float NdotVL = NdotV * NdotL;
    float geometryLightDenom =
        NdotL * (1.0 - geometryK) + geometryK;
    float specularScale =
        (roughness4 * NdotVL) /
        (PI * distributionDenom * distributionDenom *
         geometryViewDenom * geometryLightDenom *
         (4.0 * NdotVL + 0.0001));
    vec3 specular = F * specularScale;
    vec3 diffuse = (vec3(1.0) - F) * diffuseBase;
    return (diffuse + specular) * NdotL;
}

// ============================================================================
// Lighting Calculations (Cook-Torrance PBR)
// ============================================================================

vec3 calculateDirectionLight(
    vec3 N, vec3 V, vec3 diffuseBase,
    vec3 F0, float NdotV, float roughness4, float geometryK,
    float geometryViewDenom)
{
    vec3 direction = u_dirLightDir.xyz;
    float intensity = u_dirLightDir.w;
    vec3 diffuseColor = u_dirLightDiffuse.xyz;

    // Light directions are normalized once on the CPU before UBO upload.
    vec3 L = -direction;
    float NdotL = max(dot(N, L), 0.0);
    if (NdotL <= 0.0) {
        return vec3(0.0);
    }

    vec3 brdf = evaluatePBR(
        N, V, L, NdotL, diffuseBase, F0,
        NdotV, roughness4, geometryK, geometryViewDenom);
    return brdf * diffuseColor * intensity;
}

vec3 calculatePointLight(
    int lightIdx, vec3 N, vec3 V, highp vec3 fragPos, vec3 diffuseBase,
    vec3 F0, float NdotV, float roughness4, float geometryK,
    float geometryViewDenom)
{
    PointLightGPU light = u_pointLights[lightIdx];
    highp vec3 position = light.positionInvRange.xyz;
    float invRange = light.positionInvRange.w;
    vec3 diffuseColor = light.diffuseLinear.xyz;
    float linearCoef = light.diffuseLinear.w;
    float constant = light.attenuationIntensity.x;
    float quadratic = light.attenuationIntensity.y;
    float intensity = light.attenuationIntensity.z;
    highp vec3 toLight = position - fragPos;
    highp float distSq = dot(toLight, toLight);

    // Reject by squared distance before paying for normalization/PBR.
    if (invRange > 0.0 && distSq * invRange * invRange > 1.0) {
        return vec3(0.0);
    }

    highp float invDist = inversesqrt(max(distSq, 0.00000001));
    float dist = distSq * invDist;
    vec3 L = toLight * invDist;
    float NdotL = max(dot(N, L), 0.0);

    // Early-out: back-facing surface — no contribution
    if (NdotL <= 0.0) {
        return vec3(0.0);
    }

    float attenuation = 1.0 / (constant + linearCoef * dist + quadratic * distSq);

    if (invRange > 0.0) {
        float nd = dist * invRange;
        float rangeAtten = max(0.0, 1.0 - nd * nd);
        attenuation *= rangeAtten * rangeAtten;
    }

    vec3 brdf = evaluatePBR(
        N, V, L, NdotL, diffuseBase, F0,
        NdotV, roughness4, geometryK, geometryViewDenom);
    return brdf * diffuseColor * attenuation * intensity;
}

vec3 calculateSpotlight(
    int lightIdx, vec3 N, vec3 V, highp vec3 fragPos, vec3 diffuseBase,
    vec3 F0, float NdotV, float roughness4, float geometryK,
    float geometryViewDenom)
{
    SpotLightGPU light = u_spotLights[lightIdx];
    highp vec3 position = light.positionCutoff.xyz;
    vec3 direction = light.directionOuter.xyz;
    float outerCutOff = light.directionOuter.w;
    vec3 diffuseColor = light.diffuseLinear.xyz;
    float linearCoef = light.diffuseLinear.w;
    float constant = light.attenuationIntensity.x;
    float quadratic = light.attenuationIntensity.y;
    float intensity = light.attenuationIntensity.z;
    float invCutoffWidth = light.attenuationIntensity.w;

    highp vec3 toLight = position - fragPos;
    highp float distSq = dot(toLight, toLight);
    highp float invDist = inversesqrt(max(distSq, 0.00000001));
    float dist = distSq * invDist;
    vec3 L = toLight * invDist;
    float NdotL = max(dot(N, L), 0.0);

    // Spot directions are normalized once on the CPU before UBO upload.
    float theta      = dot(L, -direction);
    float spotFactor = clamp(
        (theta - outerCutOff) * invCutoffWidth, 0.0, 1.0);

    // Most fragments are outside a spotlight cone or face away from it.
    if (NdotL <= 0.0 || spotFactor <= 0.0) {
        return vec3(0.0);
    }

    float attenuation = 1.0 / (constant + linearCoef * dist + quadratic * distSq);
    vec3 brdf = evaluatePBR(
        N, V, L, NdotL, diffuseBase, F0,
        NdotV, roughness4, geometryK, geometryViewDenom);
    return brdf * diffuseColor * attenuation * spotFactor * intensity;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    vec2 tiledUV = TexCoords;
    uint materialFeatures = u_materialFeatures.x;
    bool hasDiffuseMap =
        (materialFeatures & MATERIAL_FEATURE_DIFFUSE) != 0u;
    bool hasNormalMap =
        (materialFeatures & MATERIAL_FEATURE_NORMAL) != 0u;
    bool hasEmissiveMap =
        (materialFeatures & MATERIAL_FEATURE_EMISSIVE) != 0u;
    bool hasAOMap = (materialFeatures & MATERIAL_FEATURE_AO) != 0u;
    bool hasMetallicMap =
        (materialFeatures & MATERIAL_FEATURE_METALLIC) != 0u;
    bool hasRoughnessMap =
        (materialFeatures & MATERIAL_FEATURE_ROUGHNESS) != 0u;
    bool hasOpacityMap =
        (materialFeatures & MATERIAL_FEATURE_OPACITY) != 0u;
    bool hasPackedORMMap =
        (materialFeatures & MATERIAL_FEATURE_PACKED_ORM) != 0u;
    bool materialIsDielectric =
        (materialFeatures & MATERIAL_FEATURE_DIELECTRIC) != 0u;

    // Sample diffuse once and reuse both alpha and RGB. The old path fetched
    // the same texture twice for every textured fragment.
    vec4 diffuseSample = vec4(1.0);
    if (hasDiffuseMap) {
        diffuseSample = texture(diffuseMap, tiledUV);
#if !defined(GAM300_OPAQUE_DIFFUSE)
        if (diffuseSample.a < 0.5) {
            discard;
        }
#endif
    }

    vec3 albedo = diffuseSample.rgb * u_materialDiffuseOpacity.rgb;
    // Counts are clamped to the shader maxima when the UBO is populated.
    int pointCount = u_lightCounts.x;
    int spotCount = u_lightCounts.y;
    uint lightMask =
        useInstancing ? vLightMask : uint(u_lightMask);

    // The CPU builds this once when uploading the lighting UBO. Animated and
    // manual draws may carry all bits, so clamp them to currently active slots.
    lightMask &= uint(u_lightCounts.z);

    bool hasDirectionalLight = u_dirLightDiffuse.w > 0.5;
    bool hasDirectLighting = hasDirectionalLight || lightMask != 0u;
    bool hasAmbientLighting = u_lightCounts.w >= 0;

    // Android packs AO/roughness/metallic into RGB when at least two maps are
    // present. This replaces two or three independent texture fetches with one.
    // Keep the fetch out of ambient-only draws when the packed map has no AO.
    bool needsPackedORM = hasPackedORMMap &&
        ((hasAOMap && hasAmbientLighting) ||
         (hasDirectLighting && (hasRoughnessMap || hasMetallicMap)));
    vec3 packedORM = needsPackedORM
        ? texture(packedORMMap, tiledUV).rgb
        : vec3(1.0);
    float ao = u_materialRoughnessTermsAO.z;
    if (hasAOMap && hasAmbientLighting) {
        ao *= hasPackedORMMap
            ? packedORM.r
            : texture(aoMap, tiledUV).r;
    }

    bool needsSurfaceNormal =
        hasDirectLighting || u_lightCounts.w == 1;
    vec3 norm = needsSurfaceNormal
        ? getNormalFromMap(tiledUV, hasNormalMap)
        : vec3(0.0, 1.0, 0.0);
    vec3 result = hasAmbientLighting
        ? calculateAmbient(norm) * albedo * ao
        : vec3(0.0);

    // Metallic/roughness textures, view normalization, and Cook-Torrance setup
    // have no effect on ambient-only fragments. Local-light masks are flat per
    // object, so this branch is coherent across each rendered primitive.
    if (hasDirectLighting) {
        vec3 viewDir = normalize(cameraPos - FragPos);
        float metallic = u_materialEmissiveMetallic.w;
        if (hasMetallicMap) {
            metallic = hasPackedORMMap
                ? packedORM.b
                : texture(metallicMap, tiledUV).r;
        }
        vec3 F0;
        vec3 diffuseBase;
        if (materialIsDielectric) {
            F0 = vec3(0.04);
            diffuseBase = albedo;
        } else {
            F0 = mix(vec3(0.04), albedo, metallic);
            diffuseBase = albedo * (1.0 - metallic);
        }
        float NdotV = max(dot(norm, viewDir), 0.0);
        float roughness4;
        float geometryK;
        if (hasRoughnessMap) {
            float roughness = max(
                hasPackedORMMap
                    ? packedORM.g
                    : texture(roughnessMap, tiledUV).r,
                0.1);
            float roughness2 = roughness * roughness;
            roughness4 = roughness2 * roughness2;
            float geometryR = roughness + 1.0;
            geometryK = geometryR * geometryR * 0.125;
        } else {
            roughness4 = u_materialRoughnessTermsAO.x;
            geometryK = u_materialRoughnessTermsAO.y;
        }
        float geometryViewDenom =
            NdotV * (1.0 - geometryK) + geometryK;

        if (hasDirectionalLight) {
            result += calculateDirectionLight(
                norm, viewDir, diffuseBase,
                F0, NdotV, roughness4, geometryK, geometryViewDenom);
        }

        for (int i = 0; i < pointCount; i++) {
            if ((lightMask & (1u << uint(i))) == 0u) {
                continue;
            }
            result += calculatePointLight(
                i, norm, viewDir, FragPos, diffuseBase,
                F0, NdotV, roughness4, geometryK, geometryViewDenom);
        }

        for (int i = 0; i < spotCount; i++) {
            if ((lightMask & (1u << uint(NR_POINT_LIGHTS + i))) == 0u) {
                continue;
            }
            result += calculateSpotlight(
                i, norm, viewDir, FragPos, diffuseBase,
                F0, NdotV, roughness4, geometryK, geometryViewDenom);
        }
    }

    if (hasEmissiveMap) {
        result += vec3(texture(emissiveMap, tiledUV)) *
            u_materialEmissiveMetallic.rgb;
    } else {
        result += u_materialEmissiveMetallic.rgb;
    }

    // Minimum lighting floor — no surface should ever be completely black.
    result = max(result, albedo * 0.05);

    // Nearly every entity uses the identity boost. Keep that coherent draw-wide
    // path free of three fragment multiplies.
    if (brightnessBoost != 1.0) {
        result *= brightnessBoost;
    }

    float finalAlpha = u_materialDiffuseOpacity.a * u_distanceFadeOpacity;
    if (hasOpacityMap) {
        finalAlpha *= texture(opacityMap, tiledUV).r;
    }
    FragColor = vec4(result, finalAlpha);

    // Per-entity bloom emission — written only to MRT attachment 1.
    // Modulate by fragment brightness to keep emission tied to the final color.
    float finalBloomIntensity = useInstancing ? vBloomData.a : bloomIntensity;
    vec3 finalBloomColor = useInstancing ? vBloomData.rgb : bloomColor;
    if (finalBloomIntensity > 0.0) {
        float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
        BloomEmission =
            vec4(finalBloomColor * finalBloomIntensity * brightness, 1.0);
    } else {
        BloomEmission = vec4(0.0);
    }
}
