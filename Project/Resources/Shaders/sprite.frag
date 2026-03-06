#version 330 core

in vec2 TexCoord;
layout(location = 0) out vec4 FragColor;
layout(location = 1) out vec4 BloomEmission;

uniform sampler2D spriteTexture;
uniform vec4 spriteColor;
uniform int fillMode;           // 0 = solid, 1 = radial
uniform float fillAmount;       // 0.0 to 1.0
uniform float fillGlow;         // edge glow intensity
uniform float fillBackground;   // unfilled area brightness (0=hidden, 0.3=dark, 1=full)

// Per-entity bloom emission
uniform float bloomIntensity;
uniform vec3 bloomColor;

void main()
{
    float dimFactor = 1.0;
    float edgeGlow = 0.0;

    if (fillMode == 1) {
        vec2 center = TexCoord - vec2(0.5);
        float angle = atan(center.x, center.y);  // 0 at top, clockwise
        if (angle < 0.0) angle += 2.0 * 3.14159265;
        float normalizedAngle = angle / (2.0 * 3.14159265);

        if (normalizedAngle > fillAmount) {
            dimFactor = fillBackground;
        }

        // Edge glow visible on both sides of boundary (hidden when full or empty)
        if (fillAmount > 0.01 && fillAmount < 0.99) {
            float edgeDist = abs(normalizedAngle - fillAmount);
            float glowWidth = 0.04;
            edgeGlow = smoothstep(glowWidth, 0.0, edgeDist) * fillGlow;
        }
    }

    vec4 texColor = texture(spriteTexture, TexCoord);
    FragColor = texColor * spriteColor;
    FragColor *= dimFactor;
    FragColor.rgb += vec3(edgeGlow);

    // Per-entity bloom emission — written only to MRT attachment 1
    BloomEmission = vec4(bloomColor * bloomIntensity, 1.0);

    if (FragColor.a < 0.01)
        discard;
}
