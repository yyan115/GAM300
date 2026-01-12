#version 330 core

struct Material {
    // Basic properties
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    vec3 emissive;
    float shininess;
    float opacity;
    
    // Texture maps
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    sampler2D emissiveMap;
    
    // Texture availability flags
    bool hasDiffuseMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    bool hasEmissiveMap;
};

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec3 Tangent;
in vec4 FragPosLightSpace;  // NEW: Fragment position in light space

uniform Material material;

// Lighting structures
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

// Shadow mapping uniforms
uniform sampler2D shadowMap;
uniform bool shadowsEnabled;
uniform float shadowBias;
uniform float shadowNormalBias;
uniform float shadowSoftness;

out vec4 FragColor;
uniform vec3 cameraPos;

// ============================================================================
// Helper functions for materials
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
// Shadow calculation with PCF (Percentage Closer Filtering)
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
    
    // Larger PCF kernel for much softer shadows
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    
    // 7x7 kernel with larger spread
    int samples = 0;
    for (int x = -3; x <= 3; ++x) {
        for (int y = -3; y <= 3; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize * 1.5).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            samples++;
        }
    }
    
    shadow /= float(samples);
    return shadow;
}

// ============================================================================
// Lighting calculations
// ============================================================================

vec3 calculateDirectionLight(DirectionLight light, vec3 normal, vec3 viewDir, float shadow)
{
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading (Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), material.shininess);
    
    // Combine components
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    // Apply shadow to diffuse and specular only (ambient is always visible)
    // shadow = 1.0 means fully in shadow, 0.0 means fully lit
    return (ambient + (1.0 - shadow) * (diffuse + specular)) * light.intensity;
}

vec3 calculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // Combine results
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular) * light.intensity;
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
    
    // Spotlight intensity
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
    if (material.hasDiffuseMap && texture(material.diffuseMap, TexCoords).a < 0.5) {
        discard;
    }
    
    vec3 norm = getNormalFromMap();
    vec3 viewDir = normalize(cameraPos - FragPos);
    vec3 lightDir = normalize(-dirLight.direction);
    
    float shadow = calculateShadow(FragPosLightSpace, norm, lightDir);

    vec3 result = calculateAmbient(norm) * getMaterialDiffuse() * 0.5;
    result += calculateDirectionLight(dirLight, norm, viewDir, shadow);
    
    for (int i = 0; i < numPointLights; i++) {
        result += calculatePointLight(pointLights[i], norm, FragPos, viewDir);
    }
    
    for (int i = 0; i < numSpotLights; i++) {
        result += calculateSpotlight(spotLights[i], norm, FragPos, viewDir);
    }

    if (material.hasEmissiveMap) {
        result += vec3(texture(material.emissiveMap, TexCoords)) * material.emissive;
    } else {
        result += material.emissive;
    }
    
    // Apply shadow directly to final result (force shadows to be visible)
    result *= (1.0 - shadow * 0.7);  // Darken shadowed areas by 70%
    
    FragColor = vec4(result, material.opacity);
}