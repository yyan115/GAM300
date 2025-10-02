#version 300 es
precision highp float;
precision highp sampler2D;

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
in vec3 Normal;
in vec3 FragPos;

uniform Material material;
uniform vec3 cameraPos;

// Lighting structures
struct DirectionLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
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
};
#define NR_SPOT_LIGHTS 16
uniform Spotlight spotLights[NR_SPOT_LIGHTS];
uniform int numSpotLights;

out vec4 FragColor;

// Helper function to get material diffuse color
vec3 getMaterialDiffuse() {
    if (material.hasDiffuseMap) {
        return texture(material.diffuseMap, TexCoords).rgb * material.diffuse;
    }
    return material.diffuse;
}

// Helper function to get material specular color
vec3 getMaterialSpecular() {
    if (material.hasSpecularMap) {
        return texture(material.specularMap, TexCoords).rgb * material.specular;
    }
    return material.specular;
}

// Helper function to get material ambient color
vec3 getMaterialAmbient() {
    return material.ambient;
}

// Get normal from normal map or use vertex normal
vec3 getNormalFromMap() {
    if (material.hasNormalMap) {
        vec3 tangentNormal = texture(material.normalMap, TexCoords).xyz * 2.0 - 1.0;
        
        // Compute TBN matrix from derivatives
        vec3 Q1 = dFdx(FragPos);
        vec3 Q2 = dFdy(FragPos);
        vec2 st1 = dFdx(TexCoords);
        vec2 st2 = dFdy(TexCoords);
        
        vec3 N = normalize(Normal);
        vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
        vec3 B = -normalize(cross(N, T));
        mat3 TBN = mat3(T, B, N);
        
        return normalize(TBN * tangentNormal);
    }
    return normalize(Normal);
}

// Calculate directional light contribution
vec3 calculateDirectionLight(DirectionLight light, vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(-light.direction);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading (Blinn-Phong)
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Combine results
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    return (ambient + diffuse + specular);
}

// Calculate point light contribution
vec3 calculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + 
                                light.quadratic * (distance * distance));    
    
    // Combine results
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular);
}

// Calculate spotlight contribution
vec3 calculateSpotlight(Spotlight light, vec3 normal, vec3 fragPos, vec3 viewDir) {
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + 
                                light.quadratic * (distance * distance));
    
    // Spotlight intensity (soft edges)
    float theta = dot(lightDir, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    // Combine results
    vec3 ambient  = light.ambient * getMaterialAmbient();
    vec3 diffuse  = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient  *= attenuation * intensity;
    diffuse  *= attenuation * intensity;
    specular *= attenuation * intensity;
    
    return (ambient + diffuse + specular);
}

void main() {
    vec3 norm = getNormalFromMap();
    vec3 viewDir = normalize(cameraPos - FragPos);
    
    // Start with directional light
    vec3 result = calculateDirectionLight(dirLight, norm, viewDir);
    
    // Add point lights
    for (int i = 0; i < numPointLights; i++) {
        result += calculatePointLight(pointLights[i], norm, FragPos, viewDir);
    }
    
    // Add spotlights
    for (int i = 0; i < numSpotLights; i++) {
        result += calculateSpotlight(spotLights[i], norm, FragPos, viewDir);
    }
    
    // Add emissive component
    if (material.hasEmissiveMap) {
        result += texture(material.emissiveMap, TexCoords).rgb * material.emissive;
    } else {
        result += material.emissive;
    }
    
    FragColor = vec4(result, material.opacity);
}