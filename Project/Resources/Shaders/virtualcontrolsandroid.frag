#version 300 es
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform vec4 uColor;
uniform float uAlpha;
uniform int uDirection; // 0=up, 1=left, 2=down, 3=right

// Check if point is inside a triangle pointing in given direction
bool isInsideTriangle(vec2 p, int direction) {
    vec2 center = vec2(0.5, 0.5);
    vec2 localP = p - center;

    // Triangle vertices (pointing up by default)
    vec2 v1, v2, v3;
    float size = 0.25; // Triangle size

    if (direction == 0) { // W - up
        v1 = vec2(0.0, size);
        v2 = vec2(-size, -size);
        v3 = vec2(size, -size);
    } else if (direction == 1) { // A - left
        v1 = vec2(-size, 0.0);
        v2 = vec2(size, size);
        v3 = vec2(size, -size);
    } else if (direction == 2) { // S - down
        v1 = vec2(0.0, -size);
        v2 = vec2(-size, size);
        v3 = vec2(size, size);
    } else { // D - right
        v1 = vec2(size, 0.0);
        v2 = vec2(-size, size);
        v3 = vec2(-size, -size);
    }

    // Check if point is inside triangle using barycentric coordinates
    float d1 = (localP.x - v2.x) * (v1.y - v2.y) - (v1.x - v2.x) * (localP.y - v2.y);
    float d2 = (localP.x - v3.x) * (v2.y - v3.y) - (v2.x - v3.x) * (localP.y - v3.y);
    float d3 = (localP.x - v1.x) * (v3.y - v1.y) - (v3.x - v1.x) * (localP.y - v1.y);

    bool hasNeg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
    bool hasPos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);

    return !(hasNeg && hasPos);
}

void main() {
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(TexCoord, center);

    // Circle background
    if (dist > 0.45) {
        discard; // Outside circle
    }

    vec3 color = uColor.rgb;

    // Draw triangle inside
    if (isInsideTriangle(TexCoord, uDirection)) {
        // Make triangle white/lighter
        color = vec3(1.0, 1.0, 1.0);
    }

    // Border effect
    float border = smoothstep(0.4, 0.45, dist);
    color = mix(color, vec3(0.2, 0.2, 0.2), border);

    FragColor = vec4(color, uAlpha);
}