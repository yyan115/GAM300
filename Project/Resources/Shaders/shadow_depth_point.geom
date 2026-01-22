#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;

uniform mat4 shadowMatrices[6];

out vec4 FragPos; // FragPos from GS (output per EmitVertex)

void main()
{
    // Render to all 6 faces of the cubemap
    for (int face = 0; face < 6; ++face)
    {
        // gl_Layer specifies which face of the cubemap we render to
        // 0 = +X, 1 = -X, 2 = +Y, 3 = -Y, 4 = +Z, 5 = -Z
        gl_Layer = face;
        
        // Process each vertex of the triangle
        for (int i = 0; i < 3; ++i)
        {
            // Pass world position to fragment shader for distance calculation
            FragPos = gl_in[i].gl_Position;
            
            // Transform to light's clip space for this face
            gl_Position = shadowMatrices[face] * FragPos;
            
            EmitVertex();
        }
        EndPrimitive();
    }
}
