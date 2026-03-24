#version 300 es
precision highp float;
precision highp int;

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

out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec3 color;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

layout(std140) uniform CameraBlock {
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
    float _pad;
};

uniform mat4 model;
uniform mat4 lightSpaceMatrix;
uniform mat3 normalMatrix;  // Pass this from CPU instead of computing inverse()

uniform bool useInstancing;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool hasBones;

void main()
{
    // Determine which model matrix to use
    mat4 modelMatrix;
    mat3 nrmMatrix;

    if (useInstancing)
    {
        // Reconstruct model matrix from instance attributes
        modelMatrix = mat4(
            aInstanceModelCol0,
            aInstanceModelCol1,
            aInstanceModelCol2,
            aInstanceModelCol3
        );

        // Reconstruct normal matrix from instance attributes
        nrmMatrix = mat3(
            aInstanceNormalCol0.xyz,
            aInstanceNormalCol1.xyz,
            aInstanceNormalCol2.xyz
        );
    }
    else
    {
        modelMatrix = model;
        nrmMatrix = normalMatrix;
    }

    // Start in object space
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;
    vec3 tan = aTangent;

    if (hasBones && !useInstancing)
    {
        // LBS skinning
        mat4 skin = mat4(0.0);
        float wsum = 0.0;
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
        {
            // Cast float to int for bone index (required in GLSL ES)
            int id = int(aBoneIds[i]);
            float w = aWeights[i];
            if (id >= 0 && id < MAX_BONES && w > 0.0)
            {
                skin += finalBonesMatrices[id] * w;
                wsum += w;
            }
        }
        if (wsum == 0.0) skin = mat4(1.0);

        pos = skin * pos;
        nrm = normalize(mat3(skin) * nrm);
        tan = normalize(mat3(skin) * tan);
    }

    // Transform to world space
    vec4 worldPos = modelMatrix * pos;

    FragPos = worldPos.xyz;
    Normal  = normalize(nrmMatrix * nrm);
    Tangent = normalize(nrmMatrix * tan);
    TexCoords = aTexCoord;
    color = aColor;

    // Calculate position in light space for shadow mapping
    FragPosLightSpace = lightSpaceMatrix * worldPos;

    gl_Position = projection * view * worldPos;
}
