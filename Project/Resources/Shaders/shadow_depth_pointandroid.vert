#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec3 aPos;
// OpenGL ES doesn't support ivec4 attributes well, use vec4 and cast to int
layout (location = 5) in vec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

out vec3 FragPos;  // World position for distance calculation

uniform mat4 lightSpaceMatrix;  // View-projection for current cubemap face
uniform mat4 model;

// Animation support
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool isAnimated;

void main()
{
    vec4 pos = vec4(aPos, 1.0);

    // Apply skeletal animation if present
    if (isAnimated)
    {
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
    }

    // Calculate world position
    vec4 worldPos = model * pos;
    FragPos = worldPos.xyz;

    // Transform to light's clip space for this cubemap face
    gl_Position = lightSpaceMatrix * worldPos;
}
