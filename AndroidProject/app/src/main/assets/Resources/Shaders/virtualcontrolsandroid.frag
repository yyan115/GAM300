#version 300 es
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform vec4 uColor;
uniform float uAlpha;

void main() {
    // Simple circular button shape
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(TexCoord, center);
    
    if (dist > 0.4) {
        discard; // Outside circle
    }
    
    // Border effect
    float border = smoothstep(0.35, 0.4, dist);
    vec3 color = mix(uColor.rgb, vec3(1.0), border);
    
    FragColor = vec4(color, uAlpha);
}