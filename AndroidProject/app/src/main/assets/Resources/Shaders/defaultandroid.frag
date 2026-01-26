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
    
    // PBR properties
    //float metallic;
    //float roughness;
    //float ao;
    
    // Texture maps
    sampler2D diffuseMap;
    sampler2D specularMap;
    sampler2D normalMap;
    //sampler2D heightMap;
    //sampler2D aoMap;
    //sampler2D metallicMap;
    //sampler2D roughnessMap;
    sampler2D emissiveMap;
    
    // Texture availability flags
    bool hasDiffuseMap;
    bool hasSpecularMap;
    bool hasNormalMap;
    //bool hasHeightMap;
    //bool hasAOMap;
    //bool hasMetallicMap;
    //bool hasRoughnessMap;
    bool hasEmissiveMap;
};

in vec2 TexCoords;
uniform Material material;

// Lighting structures (same as before)
struct DirectionLight{
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform DirectionLight dirLight;

struct PointLight{
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

struct Spotlight{
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
in vec3 Normal;
in vec3 Tangent;
in vec3 FragPos;
uniform vec3 cameraPos;

// Helper function to get material diffuse color
vec3 getMaterialDiffuse() {
    if (material.hasDiffuseMap) {
        return vec3(texture(material.diffuseMap, TexCoords)) * material.diffuse;
    }
    return material.diffuse;
}

// Helper function to get material specular color
vec3 getMaterialSpecular() {
    if (material.hasSpecularMap) {
        return vec3(texture(material.specularMap, TexCoords)) * material.specular;
    }
    return material.specular;
}

// Helper function to get material ambient color
vec3 getMaterialAmbient() {
    return material.ambient;
}

vec3 getNormalFromMap()
{
    if (material.hasNormalMap)
    {
        // Sample XY from normal map, remap from [0,1] → [-1,1]
        vec2 enc = texture(material.normalMap, TexCoords).rg * 2.0 - 1.0;

        // Reconstruct Z
        float z = sqrt(max(0.0, 1.0 - dot(enc, enc)));
        vec3 tangentNormal = normalize(vec3(enc, z));

        // Construct TBN
        vec3 N = normalize(Normal);
        vec3 T = normalize(Tangent);
        vec3 B = cross(N, T);

        mat3 TBN = mat3(T, B, N);

        return normalize(TBN * tangentNormal);
    }
    return normalize(Normal);
}

vec3 calculateDirectionLight(DirectionLight light, vec3 normal, vec3 view_direction)
{
    vec3 light_direction = normalize(-light.direction);
    
    // Diffuse shading
    float diff = max(dot(normal, light_direction), 0.0);
    
    // Specular shading
    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), material.shininess);
    
    vec3 ambient  = light.ambient  * getMaterialAmbient();
    vec3 diffuse  = light.diffuse  * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    return (ambient + diffuse + specular);
}

vec3 calculatePointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 view_direction)
{
    vec3 lightDir = normalize(light.position - fragPos);
    
    // Diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Specular shading
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(view_direction, reflectDir), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    
    // Combine results
    vec3 ambient  = light.ambient  * getMaterialAmbient();
    vec3 diffuse  = light.diffuse  * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular);
}

vec3 calculateSpotlight(Spotlight light, vec3 normal, vec3 fragPos, vec3 view_direction)
{
    vec3 light_direction = normalize(light.position - fragPos);
    
    // Diffuse
    float diff = max(dot(normal, light_direction), 0.0);
    
    // Specular
    vec3 reflect_direction = reflect(-light_direction, normal);
    float spec = pow(max(dot(view_direction, reflect_direction), 0.0), material.shininess);
    
    // Attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    
    // Spotlight intensity
    float theta = dot(light_direction, normalize(-light.direction));
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    
    vec3 ambient = light.ambient * getMaterialAmbient();
    vec3 diffuse = light.diffuse * diff * getMaterialDiffuse();
    vec3 specular = light.specular * spec * getMaterialSpecular();
    
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    
    return (ambient + diffuse + specular);
}

void main()
{
    if (texture(material.diffuseMap, TexCoords).a < 0.5) discard;
    
    vec3 norm = getNormalFromMap();
    vec3 viewDir = normalize(cameraPos - FragPos);
    
    // Calculate lighting
    vec3 result = calculateDirectionLight(dirLight, norm, viewDir);
    
    // Point lights loop
    for(int i = 0; i < numPointLights; i++)
        result += calculatePointLight(pointLights[i], norm, FragPos, viewDir);    
    
    // Spotlight loop
    for(int i = 0; i < numSpotLights; i++)
        result += calculateSpotlight(spotLights[i], norm, FragPos, viewDir); 
    //result += calculateSpotlight(spotLight, norm, FragPos, viewDir);

    // Add emissive component
    if (material.hasEmissiveMap) {
        result += vec3(texture(material.emissiveMap, TexCoords)) * material.emissive;
    } else {
        result += material.emissive;
    }
    
    FragColor = vec4(result, material.opacity);
}