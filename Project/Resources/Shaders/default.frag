#version 330 core

// ============================================================================
// UNIFORM BUFFER OBJECT - All lighting data in one block
// Uses std140 layout for predictable memory alignment
// ============================================================================

#define MAX_POINT_LIGHTS_UBO 16
#define MAX_SPOT_LIGHTS_UBO 16
#define MAX_POINT_LIGHT_SHADOWS 8

// Point light shadow maps (still need individual samplers)
uniform samplerCube pointShadowMaps[MAX_POINT_LIGHT_SHADOWS];
uniform float pointShadowFarPlane;

// Directional shadow map
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;

// UBO structs must match C++ exactly with std140 padding
struct PointLightData {
    vec4 position;      // xyz = position
    vec4 ambient;       // xyz = ambient
    vec4 diffuse;       // xyz = diffuse
    vec4 specular;      // xyz = specular
    float constant;
    float linear;
    float quadratic;
    float intensity;
    int shadowIndex;
    float padding[3];
};

struct SpotLightData {
    vec4 position;      // xyz = position
    vec4 direction;     // xyz = direction
    vec4 ambient;       // xyz = ambient
    vec4 diffuse;       // xyz = diffuse
    vec4 specular;      // xyz = specular
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
    float intensity;
    float padding[2];
};

struct DirectionalLightData {
    vec4 direction;     // xyz = direction
    vec4 ambient;       // xyz = ambient
    vec4 diffuse;       // xyz = diffuse
    vec4 specular;      // xyz = specular
    float intensity;
    float padding[3];
};

// The uniform block - binding point 0
layout(std140) uniform LightingData {
    vec4 ambientSky;
    vec4 ambientEquator;
    vec4 ambientGround;
    int ambientMode;
    float ambientIntensity;
    int numPointLights;
    int numSpotLights;
    
    DirectionalLightData dirLight;
    PointLightData pointLights[MAX_POINT_LIGHTS_UBO];
    SpotLightData spotLights[MAX_SPOT_LIGHTS_UBO];
};

// ============================================================================
// Material (unchanged)
// ============================================================================
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 emissive;
    float shininess;
    float opacity;
    
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D emissiveMap;
    
    bool hasDiffuseMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    bool hasEmissiveMap;
};

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec4 FragPosLightSpace;

uniform Material material;
uniform vec3 cameraPos;

out vec4 FragColor;

// ============================================================================
// Helper functions for materials (unchanged)
// ============================================================================

vec3 getMaterialDiffuse() {
    if (material.hasDiffuseMap) {
        return vec3(texture(material.diffuseMap, TexCoords)) * material.diffuse;
    }
    return material.diffuse;
}

vec3 getMaterialSpecular() {
    if (material.hasSpecularMap) {
        return vec3(texture(material.specularMap, TexCoords)) * material.specular;
    }
    return material.specular;
}

vec3 getMaterialAmbient() {
    return material.ambient;
}

vec3 calculateAmbient(vec3 normal) {
    vec3 ambient;

    if (ambientMode == 0) {
        ambient = ambientSky.rgb;
    } else if (ambientMode == 1) {
        float t = normal.y * 0.5 + 0.5;
        if (t < 0.5) {
            ambient = mix(ambientGround.rgb, ambientEquator.rgb, t * 2.0);
        } else {
            ambient = mix(ambientEquator.rgb, ambientSky.rgb, (t - 0.5) * 2.0);
        }
    } else {
        ambient = ambientSky.rgb;
    }

    return ambient * ambientIntensity;
}

vec3 getNormalFromMap() {
    if (material.hasNormalMap) {
        vec2 enc = texture(material.normalMap, TexCoords).rg * 2.0 - 1.0;
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
// Shadow Calculations (unchanged)
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
    
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    
    shadow /= 9.0;
    return shadow;
}

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
    else if (shadowIndex == 3) closestDepth = texture(pointShadowMaps[3], fragToLight).r;
    else if (shadowIndex == 4) closestDepth = texture(pointShadowMaps[4], fragToLight).r;
    else if (shadowIndex == 5) closestDepth = texture(pointShadowMaps[5], fragToLight).r;
    else if (shadowIndex == 6) closestDepth = texture(pointShadowMaps[6], fragToLight).r;
    else                       closestDepth = texture(pointShadowMaps[7], fragToLight).r;
    
    closestDepth *= pointShadowFarPlane;
    
    float bias = 0.15;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    
    return shadow;
}

// ============================================================================
// Lighting Calculations (updated to use UBO struct)
// ============================================================================

vec3 calculateDirectionLight(vec3 normal, vec3 viewDir, float shadow)
{
    vec3 lightDir = normalize(-dirLight.direction.xyz);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    
    vec3 ambient  = dirLight.ambient.rgb * getMaterialAmbient();
    vec3 diffuse  = dirLight.diffuse.rgb * diff * getMaterialDiffuse();
    vec3 specular = dirLight.specular.rgb * spec * getMaterialSpecular();
    
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * dirLight.intensity;
}

vec3 calculatePointLight(int index, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightPos = pointLights[index].position.xyz;
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (pointLights[index].constant + 
                               pointLights[index].linear * distance + 
                               pointLights[index].quadratic * (distance * distance));    
    
    float shadow = calculatePointShadow(pointLights[index].shadowIndex, fragPos, lightPos);
    
    vec3 ambient  = pointLights[index].ambient.rgb * getMaterialAmbient();
    vec3 diffuse  = pointLights[index].diffuse.rgb * diff * getMaterialDiffuse();
    vec3 specular = pointLights[index].specular.rgb * spec * getMaterialSpecular();
    
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * pointLights[index].intensity;
}

vec3 calculateSpotlight(int index, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightPos = spotLights[index].position.xyz;
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(normal, lightDir), 0.0);
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    float distance = length(lightPos - fragPos);
    float attenuation = 1.0 / (spotLights[index].constant + 
                               spotLights[index].linear * distance + 
                               spotLights[index].quadratic * (distance * distance));
    
    float theta = dot(lightDir, normalize(-spotLights[index].direction.xyz));
    float epsilon = spotLights[index].cutOff - spotLights[index].outerCutOff;
    float intensity = clamp((theta - spotLights[index].outerCutOff) / epsilon, 0.0, 1.0);
    
    vec3 ambient = spotLights[index].ambient.rgb * getMaterialAmbient();
    vec3 diffuse = spotLights[index].diffuse.rgb * diff * getMaterialDiffuse();
    vec3 specular = spotLights[index].specular.rgb * spec * getMaterialSpecular();
    
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    
    return (ambient + diffuse + specular) * spotLights[index].intensity;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    if (material.hasDiffuseMap && texture(material.diffuseMap, TexCoords).a < 0.5) {
        discard;
    }
    
    vec3 norm = getNormalFromMap();
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 lightDir = normalize(-dirLight.direction.xyz);
    
    float dirShadow = calculateShadow(FragPosLightSpace, norm, lightDir);

    vec3 result = calculateAmbient(norm) * getMaterialDiffuse() * 0.5;
    result += calculateDirectionLight(norm, viewDir, dirShadow);
    
    // Use numPointLights from UBO
    for (int i = 0; i < numPointLights; i++) {
        result += calculatePointLight(i, norm, FragPos, viewDir);
    }
    
    // Use numSpotLights from UBO
    for (int i = 0; i < numSpotLights; i++) {
        result += calculateSpotlight(i, norm, FragPos, viewDir);
    }

    if (material.hasEmissiveMap) {
        result += vec3(texture(material.emissiveMap, TexCoords)) * material.emissive;
    } else {
        result += material.emissive;
    }
    
    result *= (1.0 - dirShadow * 0.7);
    
    FragColor = vec4(result, material.opacity);
}
