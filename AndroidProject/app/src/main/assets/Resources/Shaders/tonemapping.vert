#version 330 core

layout (location = 0) in vec2 aPos;        // Position in NDC space (-1 to 1)
layout (location = 1) in vec2 aTexCoords;  // Texture coordinates (0 to 1)

out vec2 TexCoords;

void main()
{
    // Pass through texture coordinates (no transformation needed)
    TexCoords = aTexCoords;
    
    // Set vertex position directly in clip space
    // No need for model/view/projection matrices since we're already in NDC
    // z = 0.0 (flat on screen), w = 1.0 (no perspective division)
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}
