#version 330 core
layout (triangles) in;
layout (triangle_strip, max_vertices = 18) out;
uniform mat4 shadowMatrices[6];
out vec4 FragPos;

bool outsideFace(vec4 a, vec4 b, vec4 c)
{
    // A triangle outside one common homogeneous clip plane cannot cover a pixel.
    return (a.x < -a.w && b.x < -b.w && c.x < -c.w) ||
           (a.x >  a.w && b.x >  b.w && c.x >  c.w) ||
           (a.y < -a.w && b.y < -b.w && c.y < -c.w) ||
           (a.y >  a.w && b.y >  b.w && c.y >  c.w) ||
           (a.z < -a.w && b.z < -b.w && c.z < -c.w) ||
           (a.z >  a.w && b.z >  b.w && c.z >  c.w);
}

void main()
{
    for (int face = 0; face < 6; ++face)
    {
        vec4 clip[3];
        for (int i = 0; i < 3; ++i)
            clip[i] = shadowMatrices[face] * gl_in[i].gl_Position;
        if (outsideFace(clip[0], clip[1], clip[2])) continue;
        gl_Layer = face;
        for (int i = 0; i < 3; ++i)
        {
            FragPos = gl_in[i].gl_Position;
            gl_Position = clip[i];
            EmitVertex();
        }
        EndPrimitive();
    }
}
