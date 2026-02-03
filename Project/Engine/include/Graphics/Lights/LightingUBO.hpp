#pragma once
#include "../OpenGL.h"
#include <glm/glm.hpp>

// =============================================================================
// IMPORTANT: UBO struct layout must match GLSL std140 layout rules!
// - vec3 is padded to 16 bytes (use vec4 or add padding)
// - floats are 4 bytes
// - arrays have each element padded to 16 bytes
// =============================================================================

struct DirectionalLightUBO {
    glm::vec4 direction;
    glm::vec4 ambient;  
    glm::vec4 diffuse;  
    glm::vec4 specular; 
    float intensity;
    float padding[3];       // Pad to 16 bytes
};

struct PointLightUBO {
	glm::vec4 position; // w is for padding
    glm::vec4 ambient;      
    glm::vec4 diffuse;        
    glm::vec4 specular;     
    float constant;
    float linear;
    float quadratic;
    float intensity;
    int shadowIndex;
    float padding[3];       // Pad to 16 bytes
};

struct SpotLightUBO {
    glm::vec4 position;  
    glm::vec4 direction; 
    glm::vec4 ambient;   
    glm::vec4 diffuse;   
    glm::vec4 specular;  
    float constant;
    float linear;
    float quadratic;
    float cutOff;
    float outerCutOff;
    float intensity;
    float padding[2];       // Pad to 16 bytes
};

struct LightingDataUBO {
    // Ambient lighting
    glm::vec4 ambientSky;    
    glm::vec4 ambientEquator;
    glm::vec4 ambientGround; 
    int ambientMode;
    float ambientIntensity;
    int numPointLights;
    int numSpotLights;

    // Directional light
    DirectionalLightUBO dirLight;

#ifdef __ANDROID__
    static const int MAX_POINT_LIGHTS_UBO = 8;
    static const int MAX_SPOT_LIGHTS_UBO = 8;
#else
    static const int MAX_POINT_LIGHTS_UBO = 16;  // Must match shader
    static const int MAX_SPOT_LIGHTS_UBO = 16;   // Must match shader
#endif
    PointLightUBO pointLights[MAX_POINT_LIGHTS_UBO];

    SpotLightUBO spotLights[MAX_SPOT_LIGHTS_UBO];
};

class LightingUBO {
public:
    LightingUBO() = default;
    ~LightingUBO() { Shutdown(); }

    bool Initialize();
    void Shutdown();
    
    // Update the UBO data and upload to GPU
    void Update(const LightingDataUBO& data);

    // Bind to a specific binding point (must match shader)
    void Bind(GLuint bindingPoint = 0);

    // Get the buffer ID
    GLuint GetBufferID() const { return uboID; }

    bool IsInitialized() const { return initialized; }

private:
    GLuint uboID = 0;
    bool initialized = false;
};