#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D inputTexture;
uniform bool horizontal;
uniform float blurRadius;
uniform vec2 texelSize;
uniform float intensity;    // 0-1, blend between sharp and blurred

// Gaussian weights (sigma ~ 3)
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    vec3 original = texture(inputTexture, TexCoords).rgb;
    vec3 result = original * weights[0];

    for (int i = 1; i < 5; ++i)
    {
        vec2 offset = horizontal
            ? vec2(texelSize.x * float(i) * blurRadius, 0.0)
            : vec2(0.0, texelSize.y * float(i) * blurRadius);

        result += texture(inputTexture, TexCoords + offset).rgb * weights[i];
        result += texture(inputTexture, TexCoords - offset).rgb * weights[i];
    }

    // Blend between original and blurred based on intensity
    FragColor = vec4(mix(original, result, intensity), 1.0);
}
