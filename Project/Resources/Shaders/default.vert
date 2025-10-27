#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoord;
layout (location = 4) in vec3 aTangent;

layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec3 Tangent;
out vec3 color;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat3 normalMatrix;

const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool isAnimated;

void main()
{
    vec4 localPos = vec4(aPos, 1.0);
    vec3 localNrm = aNormal;

    if (isAnimated)
    {
        // --- Position skin ---
        mat4 skin = mat4(0.0);
        float wsum = 0.0;
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) 
        {
            int id = aBoneIds[i];
            float w = aWeights[i];
            if (id >= 0 && id < MAX_BONES && w > 0.0) 
            {
                skin += finalBonesMatrices[id] * w;   // no divide
                wsum += w;
            }
        }

        if (wsum == 0.0) skin = mat4(1.0);           // fallback for unweighted verts
        localPos = skin * localPos;

        // --- Normal skin (linear blend) ---
        vec3 nacc = vec3(0.0);
        float nsum = 0.0;
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) 
        {
            int id = aBoneIds[i];
            float w = aWeights[i];
            if (id >= 0 && id < MAX_BONES && w > 0.0) 
            {
                nacc += mat3(finalBonesMatrices[id]) * aNormal * w;
                nsum += w;
            }
        }

        localNrm = (nsum > 0.0) ? normalize(nacc) : aNormal;

        // --- Tangent skin (match normal handling) ---
        vec3 tacc = vec3(0.0);
        float tsum = 0.0;
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) 
        {
            int id = aBoneIds[i];
            float w = aWeights[i];
            if (id >= 0 && id < MAX_BONES && w > 0.0) 
            {
                tacc += mat3(finalBonesMatrices[id]) * aTangent * w;
                tsum += w;
            }
        }
        Tangent = (tsum > 0.0) ? normalize(tacc) : aTangent;
    }
    else
    {
        Tangent = normalMatrix * aTangent; // static mesh path
    }


    // world transforms
    vec4 worldPos = model * localPos;
    FragPos = worldPos.xyz;
    Normal  = normalize(mat3(transpose(inverse(model))) * localNrm);

    TexCoords   = aTexCoord;
    gl_Position = projection * view * worldPos;

}