#version 330 core

// ============================================================================
// Vertex Attributes
// ============================================================================
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoord;
layout (location = 4) in vec3 aTangent;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

// Instance attributes (only used when useInstancing is true)
layout (location = 7)  in vec4 aInstanceModelCol0;
layout (location = 8)  in vec4 aInstanceModelCol1;
layout (location = 9)  in vec4 aInstanceModelCol2;
layout (location = 10) in vec4 aInstanceModelCol3;
layout (location = 11) in vec4 aInstanceNormalCol0;
layout (location = 12) in vec4 aInstanceNormalCol1;
layout (location = 13) in vec4 aInstanceNormalCol2;
layout (location = 14) in vec4 aInstanceBloomData;  // rgb=color, a=intensity

// ============================================================================
// Outputs to Fragment Shader
// ============================================================================
out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec3 color;
out vec2 TexCoords;
out vec4 FragPosLightSpace;
flat out vec4 vBloomData;

// ============================================================================
// Uniforms
// ============================================================================
// Transform matrices (used when NOT instancing)
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

// Instancing toggle
uniform bool useInstancing;

// Skeletal animation
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool hasBones;

// ============================================================================
// Main
// ============================================================================
void main()
{
    // Determine which model matrix to use
    mat4 modelMatrix;
    mat3 normalMatrix;
    
    if (useInstancing) {
        // Reconstruct model matrix from instance attributes
        modelMatrix = mat4(
            aInstanceModelCol0,
            aInstanceModelCol1,
            aInstanceModelCol2,
            aInstanceModelCol3
        );

        // Reconstruct normal matrix (only need mat3, but we stored mat4 for alignment)
        normalMatrix = mat3(
            aInstanceNormalCol0.xyz,
            aInstanceNormalCol1.xyz,
            aInstanceNormalCol2.xyz
        );

        // Pass per-instance bloom data to fragment shader
        vBloomData = aInstanceBloomData;
    } else {
        modelMatrix = model;
        // Calculate normal matrix from model matrix
        normalMatrix = mat3(transpose(inverse(modelMatrix)));

        // Non-instanced: fragment shader will use uniforms instead
        vBloomData = vec4(0.0);
    }

    // Start with object-space position and normal
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;
    vec3 tan = aTangent;

    // Apply skeletal animation if present (not supported with instancing)
    if (hasBones && !useInstancing)
    {
        mat4 skin = mat4(0.0);
        float wsum = 0.0;
        
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
        {
            int id = aBoneIds[i];
            float w = aWeights[i];
            
            if (id >= 0 && id < MAX_BONES && w > 0.0)
            {
                skin += finalBonesMatrices[id] * w;
                wsum += w;
            }
        }
        
        if (wsum == 0.0) {
            skin = mat4(1.0);
        }

        pos = skin * pos;
        nrm = normalize(mat3(skin) * nrm);
        tan = normalize(mat3(skin) * tan);
    }

    // Transform to world space
    vec4 worldPos = modelMatrix * pos;

    // Output variables
    FragPos = worldPos.xyz;
    Normal = normalize(normalMatrix * nrm);
    Tangent = normalize(normalMatrix * tan);
    TexCoords = aTexCoord;
    color = aColor;

    // Shadow mapping position
    FragPosLightSpace = lightSpaceMatrix * worldPos;

    // Final clip-space position
    gl_Position = projection * view * worldPos;
}
