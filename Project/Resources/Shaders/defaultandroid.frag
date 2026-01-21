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
};

uniform Material material;

// Samplers must be declared outside structs in OpenGL ES
uniform sampler2D diffuseMap;
uniform sampler2D specularMap;
uniform sampler2D normalMap;
uniform sampler2D emissiveMap;

// Texture availability flags (separate from struct for ES compatibility)
uniform bool hasDiffuseMap;
uniform bool hasSpecularMap;
uniform bool hasNormalMap;
uniform bool hasEmissiveMap;

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
uniform vec3 cameraPos;

// ============================================================================
// Helper functions for materials
// ============================================================================

vec3 getMaterialDiffuse() {
    if (hasDiffuseMap) {
        return vec3(texture(diffuseMap, TexCoords)) * material.diffuse;
    }
    return material.diffuse;
}

vec3 getMaterialSpecular() {
    if (hasSpecularMap) {
        return vec3(texture(specularMap, TexCoords)) * material.specular;
    }
    return material.specular;
}

vec3 getMaterialAmbient() {
    return material.ambient;
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

vec3 getNormalFromMap() {
    if (hasNormalMap) {
        vec2 enc = texture(normalMap, TexCoords).rg * 2.0 - 1.0;
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
// Directional Shadow Calculation (PCF)
// Smaller kernel for mobile performance
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
    
    // 3x3 PCF kernel for mobile performance (instead of 7x7)
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
    // No shadow if index is invalid
    if (shadowIndex < 0 || shadowIndex >= MAX_POINT_LIGHT_SHADOWS)
    {
        return 0.0;
    }
    
    // Direction from light to fragment (for cubemap sampling)
    vec3 fragToLight = fragPos - lightPos;
    
    // Current distance from light
    float currentDepth = length(fragToLight);
    
    // Sample the appropriate cubemap
    float closestDepth;
    if (shadowIndex == 0)      closestDepth = texture(pointShadowMaps[0], fragToLight).r;
    else if (shadowIndex == 1) closestDepth = texture(pointShadowMaps[1], fragToLight).r;
    else if (shadowIndex == 2) closestDepth = texture(pointShadowMaps[2], fragToLight).r;
    else                       closestDepth = texture(pointShadowMaps[3], fragToLight).r;
    
    // Convert from [0,1] to world space distance
    closestDepth *= pointShadowFarPlane;
    
    // Bias to prevent shadow acne
    float bias = 0.15;
    
    // Shadow test
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;
    
    return shadow;
}

// ============================================================================
// Lighting Calculations
// ============================================================================

vec3 calculateDirectionLight(DirectionLight light, vec3 normal, vec3 viewDir, float shadow)
{
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    // Apply shadow to diffuse and specular only
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * light.intensity;
}

vec3 calculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // Calculate point shadow
    float shadow = calculatePointShadow(light.shadowIndex, fragPos, light.position);
    
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    // Apply shadow to diffuse and specular only
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * light.intensity;
}

vec3 calculateSpotlight(Spotlight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Spotlight cone
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    vec3 ambient = light.ambient * getMaterialAmbient();
    vec3 diffuse = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    
    return (ambient + diffuse + specular) * light.intensity;
}

// ============================================================================
// Main
// ============================================================================

void main()
{
    // Alpha test
    if (hasDiffuseMap && texture(diffuseMap, TexCoords).a < 0.5) {
        discard;
    }
    
    vec3 norm = getNormalFromMap();
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 lightDir = normalize(-dirLight.direction);
    
    // Directional shadow
    float dirShadow = 0.0;
    if (shadowsEnabled) {
        dirShadow = calculateShadow(FragPosLightSpace, norm, lightDir);
    }

    // Start with ambient
    vec3 result = calculateAmbient(norm) * getMaterialDiffuse() * 0.5;
    
    // Add directional light
    result += calculateDirectionLight(dirLight, norm, viewDir, dirShadow);
    
    // Add point lights (clamped to mobile limit)
    int pointCount = min(numPointLights, NR_POINT_LIGHTS);
    for (int i = 0; i < pointCount; i++) {
        result += calculatePointLight(pointLights[i], norm, FragPos, viewDir);
    }
    
    // Add spot lights (clamped to mobile limit)
    int spotCount = min(numSpotLights, NR_SPOT_LIGHTS);
    for (int i = 0; i < spotCount; i++) {
        result += calculateSpotlight(spotLights[i], norm, FragPos, viewDir);
    }

    // Emissive
    if (hasEmissiveMap) {
        result += vec3(texture(emissiveMap, TexCoords)) * material.emissive;
    } else {
        result += material.emissive;
    }
    
    // Apply directional shadow to overall result
    result *= (1.0 - dirShadow * 0.7);
    
    FragColor = vec4(result, material.opacity);
}
