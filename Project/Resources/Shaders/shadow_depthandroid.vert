#version 300 es
precision highp float;
precision highp int;

layout (location = 0) in vec3 aPos;
// OpenGL ES doesn't support ivec4 attributes well, use vec4 and cast to int
layout (location = 5) in vec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

// Affine instance basis columns; translation is packed into their .w values.
layout (location = 7)  in vec4 aInstanceModelCol0;
layout (location = 8)  in vec4 aInstanceModelCol1;
layout (location = 9)  in vec4 aInstanceModelCol2;

uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform bool useInstancing;

// Animation support
const int MAX_BONES = 100;
const int MAX_BONE_INFLUENCE = 4;
uniform mat4 finalBonesMatrices[MAX_BONES];
uniform bool isAnimated;

void main()
{
    vec4 pos = vec4(aPos, 1.0);

    mat4 modelMatrix = useInstancing
        ? mat4(
            vec4(aInstanceModelCol0.xyz, 0.0),
            vec4(aInstanceModelCol1.xyz, 0.0),
            vec4(aInstanceModelCol2.xyz, 0.0),
            vec4(aInstanceModelCol0.w, aInstanceModelCol1.w, aInstanceModelCol2.w, 1.0))
        : model;

    // Apply skeletal animation if present (mutually exclusive with instancing)
    if (isAnimated && !useInstancing)
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

    gl_Position = lightSpaceMatrix * modelMatrix * pos;
}
