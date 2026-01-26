#version 330 core

// Fragment shader for shadow depth pass
// No output needed - we only write to the depth buffer
// OpenGL automatically writes gl_FragCoord.z to the depth buffer

void main()
{
    // Intentionally empty - depth is written automatically
    // This shader exists to complete the pipeline
}
