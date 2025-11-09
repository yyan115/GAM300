#version 300 es
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform int uIsInnerCircle; // 0=outer circle, 1=inner circle

void main() {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(TexCoord, center);

    // Discard if outside circle
    if (dist > 0.45) {
        discard;
    }

    vec3 color = uColor.rgb;

    if (uIsInnerCircle == 1) {
        // Inner circle - filled
        float border = smoothstep(0.35, 0.45, dist);
        color = mix(color, vec3(0.8, 0.8, 0.8), border * 0.3);
    } else {
        // Outer circle - filled
        float border = smoothstep(0.4, 0.45, dist);
        color = mix(color, vec3(0.15, 0.15, 0.15), border);
    }

    FragColor = vec4(color, uAlpha);
}
