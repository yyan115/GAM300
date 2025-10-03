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
uniform bool isSkinned;

void main()
{
   mat4 skin = mat4(1.0f);
   if(isSkinned)
   {
      skin = mat4(0.0);
      for(int i = 0; i < MAX_BONE_INFLUENCE; i++)
      {
         int id = aBoneIds[i];
         if (id < 0 || id >= MAX_BONES) continue;
         skin += finalBonesMatrices[id] * aWeights[i];
      }
   }

   vec4 worldPos = model * (skin * vec4(aPos, 1.0));
   FragPos = worldPos.xyz;

   mat3 normalMat = mat3(transpose(inverse(model * skin)));
   Normal = normalize(normalMat * aNormal);

   TexCoords = aTexCoord;
   gl_Position = projection * view * worldPos;
}