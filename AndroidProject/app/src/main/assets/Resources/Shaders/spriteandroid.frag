#version 300 es
precision mediump float;

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D spriteTexture;
uniform vec4 spriteColor;
uniform int fillMode;           // 0 = solid, 1 = radial, 2 = horizontal, 3 = vertical
uniform int fillDirection;      // 0 = default, 1 = reverse
uniform float fillAmount;       // 0.0 to 1.0
uniform float fillGlow;         // edge glow intensity
uniform float fillBackground;   // unfilled area brightness (0=hidden, 0.3=dark, 1=full)

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
    else if (fillMode == 2) {
        // Horizontal fill
        float coord = (fillDirection == 0) ? TexCoord.x : (1.0 - TexCoord.x);

        if (coord > fillAmount) {
            dimFactor = fillBackground;
        }

        if (fillAmount > 0.01 && fillAmount < 0.99) {
            float edgeDist = abs(coord - fillAmount);
            float glowWidth = 0.02;
            edgeGlow = smoothstep(glowWidth, 0.0, edgeDist) * fillGlow;
        }
    }
    else if (fillMode == 3) {
        // Vertical fill
        float coord = (fillDirection == 0) ? TexCoord.y : (1.0 - TexCoord.y);

        if (coord > fillAmount) {
            dimFactor = fillBackground;
        }

        if (fillAmount > 0.01 && fillAmount < 0.99) {
            float edgeDist = abs(coord - fillAmount);
            float glowWidth = 0.02;
            edgeGlow = smoothstep(glowWidth, 0.0, edgeDist) * fillGlow;
        }
    }

    vec4 texColor = texture(spriteTexture, TexCoord);
    FragColor = texColor * spriteColor;
    FragColor *= dimFactor;
    FragColor.rgb += vec3(edgeGlow);

    if (FragColor.a < 0.01)
        discard;
}
