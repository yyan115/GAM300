#version 300 es
precision highp float;
precision highp int;
precision highp sampler2D;
precision highp samplerCube;

// ============================================================================
// Point Light Shadow Uniforms
// NOTE: Point light shadows limited to 4 on mobile for performance
// ============================================================================
#define MAX_POINT_LIGHT_SHADOWS 4
uniform samplerCube pointShadowMaps[MAX_POINT_LIGHT_SHADOWS];
uniform float pointShadowFarPlane;

// ============================================================================
// Material
// OpenGL ES requires samplers outside of structs
// ============================================================================
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 emissive;
    float shininess;
    float opacity;

    // PBR properties
    float metallic;
    float roughness;
    float ao;

    vec2 uTiling;
    vec2 uOffset;
};

uniform Material material;

// Samplers must be declared outside structs in OpenGL ES
uniform sampler2D diffuseMap;
uniform sampler2D specularMap;
uniform sampler2D normalMap;
uniform sampler2D emissiveMap;
uniform sampler2D metallicMap;
uniform sampler2D roughnessMap;
uniform sampler2D aoMap;
uniform sampler2D opacityMap;

// Texture availability flags (separate from struct for ES compatibility)
uniform bool hasDiffuseMap;
uniform bool hasSpecularMap;
uniform bool hasNormalMap;
uniform bool hasEmissiveMap;
uniform bool hasMetallicMap;
uniform bool hasRoughnessMap;
uniform bool hasAOMap;
uniform bool hasOpacityMap;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec4 FragPosLightSpace;

// ============================================================================
// Lighting Structures
// ============================================================================
struct DirectionLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float intensity;
};
uniform DirectionLight dirLight;

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float intensity;
    float range;
    int shadowIndex;
};
// Reduced for mobile performance
#define NR_POINT_LIGHTS 16
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform int numPointLights;

struct Spotlight {
    vec3 position;
    vec3 direction;
    float cutOff;
    float outerCutOff;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float intensity;
};
// Reduced for mobile performance
#define NR_SPOT_LIGHTS 8
uniform Spotlight spotLights[NR_SPOT_LIGHTS];
uniform int numSpotLights;

uniform int ambientMode;
uniform vec3 ambientSky;
uniform vec3 ambientEquator;
uniform vec3 ambientGround;
uniform float ambientIntensity;

// Directional shadow mapping uniforms
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
uniform float shadowBias;
uniform float shadowNormalBias;
uniform float shadowSoftness;

// Shadow map texel size (pass from CPU - textureSize can be slow on mobile)
uniform vec2 shadowMapTexelSize;

out vec4 FragColor;

// Camera UBO (binding = 0)
layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
};

// Distance-based fade opacity (0 = invisible, 1 = fully visible)
uniform float u_distanceFadeOpacity;

// ============================================================================
// Helper functions for materials
// ============================================================================

vec3 getMaterialDiffuse(vec2 uv) {
    if (hasDiffuseMap) {
        return vec3(texture(diffuseMap, uv)) * material.diffuse;
    }
    return material.diffuse;
}

float getMaterialMetallic(vec2 uv) {
    if (hasMetallicMap) {
        return texture(metallicMap, uv).r;
    }
    return material.metallic;
}

float getMaterialRoughness(vec2 uv) {
    float r;
    if (hasRoughnessMap) {
        r = texture(roughnessMap, uv).r;
    } else {
        r = material.roughness;
    }
    return max(r, 0.1); // clamp to avoid extreme NDF values / NaN in GGX
}

float getMaterialAO(vec2 uv) {
    if (hasAOMap) {
        return texture(aoMap, uv).r * material.ao;
    }
    return material.ao;
}

vec3 calculateAmbient(vec3 normal) {
    vec3 ambient;

    if (ambientMode == 0) {
        ambient = ambientSky;
    } else if (ambientMode == 1) {
        float t = normal.y * 0.5 + 0.5;
        if (t < 0.5) {
            ambient = mix(ambientGround, ambientEquator, t * 2.0);
        } else {
            ambient = mix(ambientEquator, ambientSky, (t - 0.5) * 2.0);
        }
    } else {
        ambient = ambientSky;
    }

    return ambient * ambientIntensity;
}

vec3 getNormalFromMap(vec2 uv) {
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

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    float x = clamp(1.0 - cosTheta, 0.0, 1.0);
    float x2 = x * x;
    return F0 + (1.0 - F0) * x2 * x2 * x;
}

// ============================================================================
// Directional Shadow Calculation (PCF 3x3)
// ============================================================================

float calculateShadow(vec4 fragPosLightSpace, vec3 normal, vec3 lightDir)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0
        || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float currentDepth = projCoords.z;
    float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.001);

    float shadow = 0.0;
    vec2 texelSize = shadowMapTexelSize;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(float(x), float(y)) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }

    shadow /= 9.0;
    return shadow;
}

// ============================================================================
// Point Light Shadow Calculation
// ============================================================================

float calculatePointShadow(int shadowIndex, vec3 fragPos, vec3 lightPos)
{
    if (shadowIndex < 0 || shadowIndex >= MAX_POINT_LIGHT_SHADOWS) {
        return 0.0;
    }

    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    // Distance-based PCF: kernel grows with distance → contact = hard, far = soft.
    // 8 samples on mobile for performance.
    float distNorm   = currentDepth / pointShadowFarPlane;
    float diskRadius = mix(0.01, 0.12, distNorm);

    // 8 axis-aligned offset directions
    vec3 sampleOffsets[8];
    sampleOffsets[0] = vec3( 1, 1, 0); sampleOffsets[1] = vec3(-1, 1, 0);
    sampleOffsets[2] = vec3( 1,-1, 0); sampleOffsets[3] = vec3(-1,-1, 0);
    sampleOffsets[4] = vec3( 1, 0, 1); sampleOffsets[5] = vec3(-1, 0, 1);
    sampleOffsets[6] = vec3( 0, 1,-1); sampleOffsets[7] = vec3( 0,-1,-1);

    float bias = max(0.005, currentDepth * 0.02);
    float shadow = 0.0;

    for (int i = 0; i < 8; ++i)
    {
        vec3 sampleDir = fragToLight + normalize(sampleOffsets[i]) * diskRadius;
        float closestDepth;
        if (shadowIndex == 0)      closestDepth = texture(pointShadowMaps[0], sampleDir).r;
        else if (shadowIndex == 1) closestDepth = texture(pointShadowMaps[1], sampleDir).r;
        else if (shadowIndex == 2) closestDepth = texture(pointShadowMaps[2], sampleDir).r;
        else                       closestDepth = texture(pointShadowMaps[3], sampleDir).r;
        closestDepth *= pointShadowFarPlane;
        shadow += currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }

    return shadow / 8.0;
}

// ============================================================================
// Lighting Calculations (Cook-Torrance PBR)
// ============================================================================

vec3 calculateDirectionLight(DirectionLight light, vec3 N, vec3 V, float shadow, vec3 albedo, float metallic, float roughness)
{
    vec3 L = normalize(-light.direction);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 Lo = (kD * albedo + specular) * light.diffuse * NdotL;

    return (1.0 - shadow) * Lo * light.intensity;
}

vec3 calculatePointLight(PointLight light, vec3 N, vec3 V, vec3 fragPos, vec3 albedo, float metallic, float roughness)
{
    float dist = length(light.position - fragPos);

    // Early-out: fragment is outside this light's range — skip all expensive PBR + shadow work
    if (light.range > 0.0 && dist > light.range) {
        return vec3(0.0);
    }

    vec3 L = normalize(light.position - fragPos);
    float NdotL = max(dot(N, L), 0.0);

    // Early-out: back-facing surface — no contribution
    if (NdotL <= 0.0) {
        return vec3(0.0);
    }

    vec3 H = normalize(V + L);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    if (light.range > 0.0) {
        float nd = dist / light.range;
        float rangeAtten = max(0.0, 1.0 - nd * nd);
        attenuation *= rangeAtten * rangeAtten;
    }

    float shadow = calculatePointShadow(light.shadowIndex, fragPos, light.position);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 Lo = (kD * albedo + specular) * light.diffuse * NdotL * attenuation;

    return (1.0 - shadow) * Lo * light.intensity;
}

vec3 calculateSpotlight(Spotlight light, vec3 N, vec3 V, vec3 fragPos, vec3 albedo, float metallic, float roughness)
{
    vec3 L = normalize(light.position - fragPos);
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);

    float dist        = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * dist + light.quadratic * (dist * dist));

    float theta      = dot(L, normalize(-light.direction));
    float epsilon    = light.cutOff - light.outerCutOff;
    float spotFactor = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);
    vec3 Lo = (kD * albedo + specular) * light.diffuse * NdotL * attenuation * spotFactor;

    return Lo * light.intensity;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    vec2 tiledUV = (TexCoords * material.uTiling) + material.uOffset;

    if (hasDiffuseMap && texture(diffuseMap, tiledUV).a < 0.5) {
        discard;
    }

    vec3 norm     = getNormalFromMap(tiledUV);
    vec3 viewDir  = normalize(cameraPos - FragPos);
    vec3 lightDir = normalize(-dirLight.direction);

    vec3  albedo    = getMaterialDiffuse(tiledUV);
    float metallic  = getMaterialMetallic(tiledUV);
    float roughness = getMaterialRoughness(tiledUV);
    float ao        = getMaterialAO(tiledUV);

    float dirShadow = 0.0;
    if (shadowsEnabled) {
        dirShadow = calculateShadow(FragPosLightSpace, norm, lightDir);
    }

    vec3 result = calculateAmbient(norm) * albedo * ao;

    result += calculateDirectionLight(dirLight, norm, viewDir, dirShadow, albedo, metallic, roughness);

    int pointCount = min(numPointLights, NR_POINT_LIGHTS);
    for (int i = 0; i < pointCount; i++) {
        result += calculatePointLight(pointLights[i], norm, viewDir, FragPos, albedo, metallic, roughness);
    }

    int spotCount = min(numSpotLights, NR_SPOT_LIGHTS);
    for (int i = 0; i < spotCount; i++) {
        result += calculateSpotlight(spotLights[i], norm, viewDir, FragPos, albedo, metallic, roughness);
    }

    if (hasEmissiveMap) {
        result += vec3(texture(emissiveMap, tiledUV)) * material.emissive;
    } else {
        result += material.emissive;
    }

    float finalAlpha = material.opacity * u_distanceFadeOpacity;
    if (hasOpacityMap) {
        finalAlpha *= texture(opacityMap, tiledUV).r;
    }
    FragColor = vec4(result, finalAlpha);
}
