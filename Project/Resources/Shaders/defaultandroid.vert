#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoord;
layout (location = 4) in vec3 aTangent;

// OpenGL ES doesn't support ivec4 attributes well, use vec4 and cast to int
layout (location = 5) in vec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec3 color;
out vec2 TexCoords;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform mat3 normalMatrix;  // Pass this from CPU instead of computing inverse()

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool hasBones;

void main()
{
    // Start in object space
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;
    vec3 tan = aTangent;

    if (hasBones)
    {
        // LBS skinning
        mat4 skin = mat4(0.0);
        float wsum = 0.0;
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
        {
            // Cast float to int for bone index
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
    vec4 worldPos = model * pos;

    FragPos = worldPos.xyz;
    Normal  = normalize(normalMatrix * nrm);
    Tangent = normalize(normalMatrix * tan);
    TexCoords = aTexCoord;
    color = aColor;

    // Calculate position in light space for shadow mapping
    FragPosLightSpace = lightSpaceMatrix * worldPos;

    gl_Position = projection * view * worldPos;
}
