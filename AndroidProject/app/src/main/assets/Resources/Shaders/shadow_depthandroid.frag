#version 300 es
precision highp float;

// OpenGL ES requires an output even if we only use depth buffer
out vec4 FragColor;

// Fragment shader for shadow depth pass
// We only care about depth - but ES requires a fragment output
void main()
{
    // Output doesn't matter - depth is written automatically
    // But OpenGL ES requires we write something
    FragColor = vec4(1.0);
}
