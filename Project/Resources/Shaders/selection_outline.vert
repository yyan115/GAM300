#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4  aWeights;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float outlineThickness;

// Animation support
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool isAnimated;

void main()
{
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;

    // Apply skeletal animation if present
    if (isAnimated)
    {
        mat4 skin = mat4(0.0);
        float wsum = 0.0;
        for (int i = 0; i < MAX_BONE_INFLUENCE; ++i)
        {
            int   id = aBoneIds[i];
            float w  = aWeights[i];
            if (id >= 0 && id < MAX_BONES && w > 0.0)
            {
                skin += finalBonesMatrices[id] * w;
                wsum += w;
            }
        }
        if (wsum == 0.0) skin = mat4(1.0);
        pos = skin * pos;
        nrm = normalize(mat3(skin) * nrm);
    }

    // Extrude along normal in object space
    pos.xyz += normalize(nrm) * outlineThickness;

    gl_Position = projection * view * model * pos;
}
