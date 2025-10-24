#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec3 aColor;
layout (location = 3) in vec2 aTexCoord;

layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

out vec3 FragPos;
out vec3 Normal;
out vec3 color;
out vec2 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

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
        // position: weighted bone matrices
        mat4 skin = mat4(0.0);
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) 
        {
            int id = aBoneIds[i];
            if (id < 0 || id >= MAX_BONES) continue;
            skin += finalBonesMatrices[id] * aWeights[i];
        }
        localPos = skin * localPos;

        // normal: weight the 3x3 parts (no inverse/transpose here)
        vec3 n = vec3(0.0);
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
            int id = aBoneIds[i];
            if (id < 0 || id >= MAX_BONES) continue;
            n += mat3(finalBonesMatrices[id]) * aNormal * aWeights[i];
        }
        localNrm = normalize(n);
    }

    // world transforms
    vec4 worldPos = model * localPos;
    FragPos = worldPos.xyz;
    Normal  = normalize(mat3(transpose(inverse(model))) * localNrm);

    TexCoords   = aTexCoord;
    gl_Position = projection * view * worldPos;

}