#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in ivec4 aBoneIds;
layout (location = 6) in vec4 aWeights;

uniform mat4 model;
uniform bool isAnimated;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main()
{
    vec4 pos = vec4(aPos, 1.0);
    
    if (isAnimated)
    {
        mat4 skin = mat4(0.0);
        float wsum = 0.0;
        for (int i = 0; i < 4; ++i)
        {
            int id = aBoneIds[i];
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
    
    gl_Position = model * pos;
}