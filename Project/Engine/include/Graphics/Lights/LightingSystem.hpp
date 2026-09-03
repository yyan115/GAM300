#pragma once
#include "ECS/System.hpp"
#include "Graphics/ShaderClass.h"
#include "Graphics/Shadows/ShadowMap.hpp"
#include "Graphics/Shadows/PointShadowMap.hpp"
#include <cstdint>

struct DirectionalLightComponent;
struct PointLightComponent;
struct SpotLightComponent;
struct AABB;
class Camera;

// Shader-side array sizes for the Android lighting UBO — must match
// NR_POINT_LIGHTS / NR_SPOT_LIGHTS in defaultandroid.frag EXACTLY.
// Only used by the Android UBO path; PC keeps the old per-draw uniform path.
constexpr int LIGHTING_UBO_MAX_POINT_LIGHTS = 16;
constexpr int LIGHTING_UBO_MAX_SPOT_LIGHTS = 8;

class LightingSystem : public System {
public:
#ifdef __ANDROID__
    // Same capacity the mobile renderer shipped with before the performance
    // pass; the per-object light masks keep the fragment cost proportional to
    // the lights that actually reach each draw, so the cap does not need to
    // cut lights the desktop build shows.
    int MAX_POINT_LIGHTS = 16;
    int MAX_SPOT_LIGHTS = 8;
    const int MAX_VISIBLE_POINT_LIGHTS = 16;
#else
    int MAX_POINT_LIGHTS = 32;
    int MAX_SPOT_LIGHTS = 16;
    const int MAX_VISIBLE_POINT_LIGHTS = 24;
#endif
    static const int MAX_POINT_LIGHT_SHADOWS = 4;

    
    LightingSystem() = default;
    ~LightingSystem() = default;

    int GetMaxPointLights() const { return MAX_POINT_LIGHTS; }
    int GetMaxSpotLights() const { return MAX_SPOT_LIGHTS; }

    bool Initialise();
    void Update();
    void Shutdown();
    void ResetDefaults();

    void ApplyLighting(Shader& shader);

#ifdef __ANDROID__
    // Android uses a UBO (Uniform Buffer Object) for lighting data instead of
    // per-draw uniform uploads. This eliminates ~244 uniform setter calls per
    // draw call (a brutal bottleneck on mobile OpenGL drivers).
    //
    // The struct layout below EXACTLY matches `layout(std140) uniform LightingBlock`
    // in defaultandroid.frag. Fields are packed into vec4s to keep std140 rules
    // predictable — vec3 in std140 is 16-byte aligned which causes subtle padding
    // bugs when mixed with scalars.
    //
    // Only values consumed by the mobile PBR shader are stored here. Keeping the
    // block compact reduces uniform-cache pressure in every lit fragment.
    // Total size: 1376 bytes.
    struct alignas(16) LightingUBOData {
        // Ambient (globals)
        glm::vec4 ambSkyIntensity;   // xyz = ambientSky,     w = ambientIntensity
        glm::vec4 ambEquatorMode;    // xyz = ambientEquator, w = pad
        glm::vec4 ambGround;         // xyz = ambientGround,  w = pad

        // Directional light
        glm::vec4 dirLightDir;       // xyz = direction,      w = intensity
        glm::vec4 dirLightDiffuse;   // xyz = diffuse,        w = hasDirectionalLight

        // Light counts
        glm::ivec4 lightCounts;      // xy = counts, z = active mask,
                                     // w = ambient mode (-1 when disabled)

        // Point lights: 3 vec4s each.
        //   [0] positionInvRange:    xyz = position, w = 1/range (0 = unlimited)
        //   [1] diffuseLinear:       xyz = diffuse,  w = linear
        //   [2] attenuationIntensity: x = constant, y = quadratic,
        //                             z = intensity, w = pad
        glm::vec4 pointLights[LIGHTING_UBO_MAX_POINT_LIGHTS * 3];

        // Spot lights: 4 vec4s each.
        //   [0] positionCutoff:       xyz = position,  w = cutOff
        //   [1] directionOuter:       xyz = direction, w = outerCutOff
        //   [2] diffuseLinear:        xyz = diffuse,   w = linear
        //   [3] attenuationIntensity: x = constant, y = quadratic,
        //                             z = intensity, w = 1/(cutOff-outerCutOff)
        glm::vec4 spotLights[LIGHTING_UBO_MAX_SPOT_LIGHTS * 4];
    };
    static_assert(sizeof(LightingUBOData) == 1376,
        "Lighting UBO must match defaultandroid.frag's std140 block");

    // Allocate the UBO and bind to binding point 1 (CameraBlock uses 0).
    void InitLightingUBO();

    // Populate and upload the UBO — call once per frame AFTER CollectLightData().
    void UploadLightingUBO();

    // Getter so GraphicsManager can bind the UBO explicitly if needed.
    GLuint GetLightingUBO() const { return m_lightingUBO; }
    std::uint64_t GetLightingRevision() const { return m_lightingRevision; }

    // Returns conservative packed masks for lights that can touch the supplied
    // bounds. Point lights occupy bits 0-7 and spot lights bits 8-11.
    std::uint32_t GetLightMask(const AABB& worldBounds) const noexcept;
#endif

    // ========================================================================
    // SHADOW MAPPING - NEW
    // ========================================================================
    void RenderShadowMaps(unsigned int restoreFramebuffer, int restoreViewportWidth, int restoreViewportHeight);
    void ApplyShadows(Shader& shader);

    // Shadow settings
    bool shadowsEnabled = true;
    int shadowMapResolution = 256;
    float shadowDistance = 25.0f;  // How far shadows extend from camera
#ifdef ANDROID
    int pointShadowMapResolution = 128;
#else
    int pointShadowMapResolution = 256;
#endif
    float pointLightShadowFarPlane = 25.0f;

    void SetShadowRenderCallback(std::function<void(Shader&)> callback) {
        shadowRenderCallback = callback;
    }

    // Rebuild point shadow maps at a new resolution. quality: 0=Low(128), 1=Medium(256), 2=High(512)
    ENGINE_API void SetPointShadowQuality(int quality);

    int GetActiveShadowCasterCount() const { return activeShadowCasterCount; }

    // ========================================================================
    // AMBIENT LIGHTING
    // ========================================================================
    enum class AmbientMode {
        Color,
        Gradient,
        Skybox
    };

    AmbientMode ambientMode = AmbientMode::Color;
    glm::vec3 ambientSky = glm::vec3(0.05f, 0.05f, 0.05f);
    glm::vec3 ambientEquator = glm::vec3(0.03f, 0.03f, 0.03f);
    glm::vec3 ambientGround = glm::vec3(0.01f, 0.01f, 0.01f);
    float ambientIntensity = 1.0f;  // Multiplier for ambient lighting

    void SetAmbientMode(AmbientMode mode) { ambientMode = mode; }
    void SetAmbientSky(glm::vec3 color) { ambientSky = color; }
    void SetAmbientEquator(glm::vec3 color) { ambientEquator = color; }
    void SetAmbientGround(glm::vec3 color) { ambientGround = color; }

private:
    // Simple arrays to store light data
    struct {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> ambient;
        std::vector<glm::vec3> diffuse;
        std::vector<glm::vec3> specular;
        std::vector<float> constant;
        std::vector<float> linear;
        std::vector<float> quadratic;
        std::vector<float> intensity;
        std::vector<float> range;
        std::vector<int> shadowIndex;
    } pointLightData;

    // Single directional light data
    struct {
        bool hasDirectionalLight = false;
        glm::vec3 direction;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float intensity = 1.0f;
        //bool castShadows = true;  // NEW
    } directionalLightData;

    // Single spot light data -> Multiple spot light data
    struct {
        std::vector<glm::vec3> positions;
        std::vector<glm::vec3> directions;
        std::vector<glm::vec3> ambient;
        std::vector<glm::vec3> diffuse;
        std::vector<glm::vec3> specular;
        std::vector<float> constant;
        std::vector<float> linear;
        std::vector<float> quadratic;
        std::vector<float> cutOff;
        std::vector<float> outerCutOff;
        std::vector<float> intensity;
    } spotLightData;

    void CollectLightData();

    // Reusable temp vectors for CollectLightData (avoids per-frame heap allocation)
    struct PointLightCandidate {
        glm::vec3 position;
        glm::vec3 ambient;
        glm::vec3 diffuse;
        glm::vec3 specular;
        float constant;
        float linear;
        float quadratic;
        float intensity;
        float range;
        bool castShadows;
        float distanceSqToCamera;
    };
    std::vector<PointLightCandidate> m_allPointLights;

    struct ShadowCandidate {
        size_t lightIndex;
        float distanceSqToCamera;
    };
    std::vector<ShadowCandidate> m_shadowCandidates;

    // ========================================================================
    // SHADOW MAPPING RESOURCES - NEW
    // ========================================================================

    DirectionalShadowMap directionalShadowMap;
    std::vector<PointShadowMap> pointShadowMaps;

    // Callback to render scene for shadow pass (set by GraphicsManager)
    std::function<void(Shader&)> shadowRenderCallback;

    // Track active shadow casters for editor warnings
    int activeShadowCasterCount = 0;

#ifdef __ANDROID__
    // Lighting UBO (binding = 1) — uploaded once per frame, read by all shaders
    // that declare `layout(std140) uniform LightingBlock`.
    GLuint m_lightingUBO = 0;
    LightingUBOData m_lastUploadedLightingData{};
    bool m_hasUploadedLightingData = false;
    std::uint64_t m_lightingRevision = 0;
#endif
};
