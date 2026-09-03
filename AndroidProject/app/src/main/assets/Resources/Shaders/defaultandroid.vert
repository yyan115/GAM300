#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoord;
layout (location = 4) in vec3 aTangent;

layout (location = 5) in uvec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

// Instance attributes (only used when useInstancing is true)
layout (location = 7)  in vec4 aInstanceModelCol0;
layout (location = 8)  in vec4 aInstanceModelCol1;
layout (location = 9)  in vec4 aInstanceModelCol2;
layout (location = 11) in vec4 aInstanceNormalCol0;
layout (location = 12) in vec4 aInstanceNormalCol1;
layout (location = 13) in vec4 aInstanceNormalCol2;
layout (location = 14) in vec4 aInstanceBloomData;  // rgb=color, a=intensity
layout (location = 15) in uint aInstanceLightMask;

out highp vec3 FragPos;
out mediump vec3 Normal;
out mediump vec3 Tangent;
out mediump vec2 TexCoords;
flat out mediump vec4 vBloomData;
flat out uint vLightMask;

layout(std140) uniform CameraBlock {
    highp mat4 view;
    highp mat4 projection;
    highp vec3 cameraPos;
    highp float _pad;
    highp mat4 viewProjection;
};

uniform mat4 model;
uniform mat3 normalMatrixCPU;  // Pass this from CPU instead of computing inverse()

layout(std140) uniform MaterialBlock {
    vec4 u_materialDiffuseOpacity;
    vec4 u_materialEmissiveMetallic;
    vec4 u_materialRoughnessTermsAO;
    vec4 u_materialUVTransform; // xy = tiling, zw = offset
    uvec4 u_materialFeatures;
};

const uint MATERIAL_FEATURE_NORMAL = 1u << 1;

uniform bool useInstancing;
uniform int u_lightMask;
uniform int u_vertexActiveLightMask;
uniform bool u_vertexNeedsGlobalNormal;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
layout(std140) uniform BonesBlock {
    // Each affine bone stores three linear columns with translation packed in
    // their w components. The discarded fourth row is always (0, 0, 0, 1).
    vec4 finalBoneColumns[MAX_BONES * 3];
};
uniform bool hasBones;

void main()
{
    bool hasNormalMap =
        (u_materialFeatures.x & MATERIAL_FEATURE_NORMAL) != 0u;
    uint objectLightMask = useInstancing
        ? aInstanceLightMask
        : uint(u_lightMask);
    bool needsSurfaceBasis =
        u_vertexNeedsGlobalNormal ||
        (objectLightMask & uint(u_vertexActiveLightMask)) != 0u;

    // Determine which model matrix to use
    mat4 modelMatrix;
    mat3 nrmMatrix;

    if (useInstancing)
    {
        // Reconstruct model matrix from instance attributes
        modelMatrix = mat4(
            vec4(aInstanceModelCol0.xyz, 0.0),
            vec4(aInstanceModelCol1.xyz, 0.0),
            vec4(aInstanceModelCol2.xyz, 0.0),
            vec4(aInstanceModelCol0.w, aInstanceModelCol1.w, aInstanceModelCol2.w, 1.0)
        );

        if (needsSurfaceBasis) {
            // Reconstruct normal matrix only when a fragment-stage lighting
            // path can consume it.
            nrmMatrix = mat3(
                aInstanceNormalCol0.xyz,
                aInstanceNormalCol1.xyz,
                aInstanceNormalCol2.xyz
            );
        }

        // Pass per-instance bloom data to fragment shader
        vBloomData = aInstanceBloomData;
        vLightMask = aInstanceLightMask;
    }
    else
    {
        modelMatrix = model;
        nrmMatrix = normalMatrixCPU;

        // Non-instanced: fragment shader will use uniforms instead
        vBloomData = vec4(0.0);
        vLightMask = 0u;
    }

    // Start in object space
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;
    vec3 tan = aTangent;

    if (hasBones && !useInstancing)
    {
        // Blend only the 12 non-constant components of each affine bone. A
        // conventional mat4 blend spends another 25% of its UBO reads and
        // arithmetic repeatedly blending the constant final row.
        vec4 skinCol0 = vec4(0.0);
        vec4 skinCol1 = vec4(0.0);
        vec4 skinCol2 = vec4(0.0);
        float skinWeight = 0.0;
        // Rigidly weighted vertices are common in the shipped character meshes.
        // Their normalized u16 weights are exactly (1, 0, 0, 0), so avoid three
        // loop iterations and all blend multiplies without changing the result.
        bool singleBone = aWeights.x == 1.0 &&
            all(equal(aWeights.yzw, vec3(0.0))) &&
            aBoneIds.x < uint(MAX_BONES);
        if (singleBone) {
            int base = int(aBoneIds.x) * 3;
            skinCol0 = finalBoneColumns[base];
            skinCol1 = finalBoneColumns[base + 1];
            skinCol2 = finalBoneColumns[base + 2];
            skinWeight = 1.0;
        }
        else {
            for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
            {
                int id = int(aBoneIds[i]);
                float w = aWeights[i];
                // Bone IDs arrive as unsigned bytes (255 is the invalid sentinel),
                // so a lower-bound comparison can never succeed differently.
                if (id < MAX_BONES && w > 0.0)
                {
                    int base = id * 3;
                    skinCol0 += finalBoneColumns[base] * w;
                    skinCol1 += finalBoneColumns[base + 1] * w;
                    skinCol2 += finalBoneColumns[base + 2] * w;
                    skinWeight += w;
                }
            }
        }

        if (skinWeight > 0.0) {
            vec3 skinTranslation =
                vec3(skinCol0.w, skinCol1.w, skinCol2.w);
            pos = vec4(
                skinCol0.xyz * pos.x +
                skinCol1.xyz * pos.y +
                skinCol2.xyz * pos.z +
                skinTranslation * pos.w,
                skinWeight * pos.w);

            if (needsSurfaceBasis) {
                mat3 skinRotation = mat3(
                    skinCol0.xyz, skinCol1.xyz, skinCol2.xyz);
                // The world-space transform below normalizes the result. Doing so
                // here as well only rescales the intermediate vector.
                nrm = skinRotation * nrm;
                if (hasNormalMap) {
                    tan = skinRotation * tan;
                }
            }
        }
    }

    // Transform to world space
    vec4 worldPos = modelMatrix * pos;

    FragPos = worldPos.xyz;
    Normal = needsSurfaceBasis
        ? normalize(nrmMatrix * nrm)
        : vec3(0.0, 1.0, 0.0);
    Tangent = needsSurfaceBasis && hasNormalMap
        ? normalize(nrmMatrix * tan)
        : vec3(0.0);
    TexCoords =
        aTexCoord * u_materialUVTransform.xy + u_materialUVTransform.zw;

    gl_Position = viewProjection * worldPos;
}
