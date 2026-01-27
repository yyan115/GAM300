#version 300 es
precision highp float;

in vec3 FragPos;

uniform vec3 lightPos;
uniform float farPlane;

// OpenGL ES requires a fragment output
out vec4 FragColor;

void main()
{
    // Calculate distance from light to fragment in world space
    float lightDistance = length(FragPos - lightPos);
    
    // Normalize to [0, 1] range by dividing by far plane
    // This is what we'll sample in the main shader
    lightDistance = lightDistance / farPlane;
    
    // Write as depth value
    gl_FragDepth = lightDistance;
    
    // Dummy output (required by ES but not used since we only care about depth)
    FragColor = vec4(1.0);
}
