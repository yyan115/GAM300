#version 330 core

// ============================================================================
// Point Light Shadow Uniforms
// ============================================================================
#define MAX_POINT_LIGHT_SHADOWS 4
uniform samplerCube pointShadowMaps[MAX_POINT_LIGHT_SHADOWS];
uniform float pointShadowFarPlane;

// ============================================================================
// Material
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

    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D emissiveMap;
    sampler2D metallicMap;
    sampler2D roughnessMap;
    sampler2D aoMap;
    sampler2D heightMap;
    sampler2D opacityMap;

    bool hasDiffuseMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    bool hasEmissiveMap;
    bool hasMetallicMap;
    bool hasRoughnessMap;
    bool hasAOMap;
    bool hasHeightMap;
    bool hasOpacityMap;

    vec2 uTiling;
    vec2 uOffset;
};

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec4 FragPosLightSpace;
flat in vec4 vBloomData;

uniform Material material;
uniform bool useInstancing;

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
#define NR_POINT_LIGHTS 32
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
#define NR_SPOT_LIGHTS 16
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

layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 BloomEmission;

// Camera UBO (binding = 0)
layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
};

// Distance-based fade opacity (0 = invisible, 1 = fully visible)
uniform float u_distanceFadeOpacity;

// Per-entity bloom emission
uniform float bloomIntensity;
uniform vec3 bloomColor;

// Environment reflections
uniform sampler2D envMap;
uniform bool hasEnvMap;
uniform float envReflectionIntensity;

// ============================================================================
// Helper functions for materials
// ============================================================================

vec3 getMaterialDiffuse(vec2 _TexCoords) {
    if (material.hasDiffuseMap) {
        return vec3(texture(material.diffuseMap, _TexCoords)) * material.diffuse;
    }
    return material.diffuse;
}

vec3 getMaterialSpecular(vec2 _TexCoords) {
    if (material.hasSpecularMap) {
        return vec3(texture(material.specularMap, _TexCoords)) * material.specular;
    }
    return material.specular;
}

vec3 getMaterialAmbient() {
    return material.ambient;
}

float getMaterialMetallic(vec2 _TexCoords) {
    if (material.hasMetallicMap) {
        return texture(material.metallicMap, _TexCoords).r;
    }
    return material.metallic;
}

float getMaterialRoughness(vec2 _TexCoords) {
    if (material.hasRoughnessMap) {
        return texture(material.roughnessMap, _TexCoords).r;
    }
    return material.roughness;
}

float getMaterialAO(vec2 _TexCoords) {
    if (material.hasAOMap) {
        return texture(material.aoMap, _TexCoords).r * material.ao;
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

vec3 getNormalFromMap(vec2 _TexCoords) {
    if (material.hasNormalMap) {
        vec2 enc = texture(material.normalMap, _TexCoords).rg * 2.0 - 1.0;
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

// GGX/Trowbridge-Reitz normal distribution function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// Schlick-GGX geometry term (single direction)
float GeometrySchlickGGX(float NdotX, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

// Smith geometry function (both view and light directions)
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
         * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
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
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    // 3x3 PCF kernel (9 samples)
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
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
    if (shadowIndex < 0 || shadowIndex >= MAX_POINT_LIGHT_SHADOWS)
    {
        return 0.0;
    }

    vec3 fragToLight = fragPos - lightPos;
    float currentDepth = length(fragToLight);

    float closestDepth;
    if (shadowIndex == 0)      closestDepth = texture(pointShadowMaps[0], fragToLight).r;
    else if (shadowIndex == 1) closestDepth = texture(pointShadowMaps[1], fragToLight).r;
    else if (shadowIndex == 2) closestDepth = texture(pointShadowMaps[2], fragToLight).r;
    else                       closestDepth = texture(pointShadowMaps[3], fragToLight).r;

    closestDepth *= pointShadowFarPlane;

    // Scale bias with distance: small bias close to light, larger further away.
    // Prevents the fixed-bias dark halo when a light is near a surface.
    float bias = max(0.005, currentDepth * 0.02);
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
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

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 Lo = (kD * albedo / PI + specular) * light.diffuse * NdotL;

    return (1.0 - shadow) * Lo * light.intensity;
}

vec3 calculatePointLight(PointLight light, vec3 N, vec3 V, vec3 fragPos, vec3 albedo, float metallic, float roughness)
{
    vec3 L = normalize(light.position - fragPos);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);

    float distance    = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    // Range cutoff: smoothly fade to zero at the range boundary (range=0 means unlimited)
    if (light.range > 0.0) {
        float nd = distance / light.range;
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

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 Lo = (kD * albedo / PI + specular) * light.diffuse * NdotL * attenuation;

    return (1.0 - shadow) * Lo * light.intensity;
}

vec3 calculateSpotlight(Spotlight light, vec3 N, vec3 V, vec3 fragPos, vec3 albedo, float metallic, float roughness)
{
    vec3 L = normalize(light.position - fragPos);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);

    float distance    = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));

    float theta        = dot(L, normalize(-light.direction));
    float epsilon      = light.cutOff - light.outerCutOff;
    float spotFactor   = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float NDF = DistributionGGX(N, H, roughness);
    float G   = GeometrySmith(N, V, L, roughness);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 Lo = (kD * albedo / PI + specular) * light.diffuse * NdotL * attenuation * spotFactor;

    return Lo * light.intensity;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    if (material.hasDiffuseMap && texture(material.diffuseMap, TexCoords).a < 0.5) {
        discard;
    }

    vec2 tiledUV = (TexCoords * material.uTiling) + material.uOffset;

    // Simple parallax offset (if height map exists)
    vec3 viewDir = normalize(cameraPos - FragPos);
    if (material.hasHeightMap) {
        float height = texture(material.heightMap, tiledUV).r;
        // Offset UV in view direction proportional to height
        vec3 viewTangent = normalize(transpose(mat3(normalize(Tangent), cross(normalize(Normal), normalize(Tangent)), normalize(Normal))) * viewDir);
        tiledUV += viewTangent.xy * (height * 0.04);
    }

    vec3 norm = getNormalFromMap(tiledUV);
    vec3 lightDir = normalize(-dirLight.direction);

    // Fetch PBR properties
    vec3  albedo    = getMaterialDiffuse(tiledUV);
    float metallic  = getMaterialMetallic(tiledUV);
    float roughness = getMaterialRoughness(tiledUV);
    float ao        = getMaterialAO(tiledUV);

    float dirShadow = calculateShadow(FragPosLightSpace, norm, lightDir);

    // Ambient with AO
    vec3 result = calculateAmbient(norm) * albedo * ao;

    result += calculateDirectionLight(dirLight, norm, viewDir, dirShadow, albedo, metallic, roughness);

    for (int i = 0; i < numPointLights; i++) {
        result += calculatePointLight(pointLights[i], norm, viewDir, FragPos, albedo, metallic, roughness);
    }

    for (int i = 0; i < numSpotLights; i++) {
        result += calculateSpotlight(spotLights[i], norm, viewDir, FragPos, albedo, metallic, roughness);
    }

    if (material.hasEmissiveMap) {
        result += vec3(texture(material.emissiveMap, tiledUV)) * material.emissive;
    } else {
        result += material.emissive;
    }

    // Environment reflections (skybox equirectangular map)
    if (hasEnvMap) {
        vec3 reflectDir = reflect(-viewDir, norm);
        float u = 0.5 + atan(reflectDir.z, reflectDir.x) / (2.0 * 3.14159265359);
        float v = 0.5 - asin(clamp(reflectDir.y, -1.0, 1.0)) / 3.14159265359;
        vec3 envColor = texture(envMap, vec2(u, v)).rgb;

        // Fresnel (Schlick approximation)
        float cosTheta = max(dot(norm, viewDir), 0.0);
        float fresnel = 0.04 + 0.96 * pow(1.0 - cosTheta, 5.0);

        // Metallic surfaces reflect strongly; roughness dims reflections
        float reflectStrength = mix(fresnel * 0.3, fresnel, metallic) * (1.0 - roughness * 0.9);
        result = mix(result, envColor, reflectStrength * envReflectionIntensity);
    }

    float finalAlpha = material.opacity * u_distanceFadeOpacity;
    if (material.hasOpacityMap) {
        finalAlpha *= texture(material.opacityMap, tiledUV).r;
    }
    FragColor = vec4(result, finalAlpha);

    // Per-entity bloom emission — written only to MRT attachment 1
    // Modulate by fragment brightness so shadowed areas don't glow
    float finalBloomIntensity = useInstancing ? vBloomData.a : bloomIntensity;
    vec3 finalBloomColor = useInstancing ? vBloomData.rgb : bloomColor;
    float brightness = dot(result, vec3(0.2126, 0.7152, 0.0722));
    BloomEmission = vec4(finalBloomColor * finalBloomIntensity * brightness, 1.0);
}
