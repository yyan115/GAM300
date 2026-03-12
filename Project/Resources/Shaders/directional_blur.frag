#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D inputTexture;
uniform float intensity;       // 0-1 blend between sharp and blurred
uniform float blurStrength;    // pixel distance of blur spread
uniform vec2 blurDirection;    // normalized direction vector in screen space
uniform vec2 texelSize;
uniform int samples;           // number of samples per side (total = 2*samples+1)

void main()
{
    vec3 original = texture(inputTexture, TexCoords).rgb;

    // Step size along the blur direction in UV space
    vec2 step = blurDirection * texelSize * blurStrength;

    // Accumulate samples along the direction with a simple tent/linear falloff
    vec3 result = original;
    float totalWeight = 1.0;

    for (int i = 1; i <= samples; ++i)
    {
        float weight = 1.0 - float(i) / float(samples + 1);
        vec2 offset = step * float(i);

        result += texture(inputTexture, TexCoords + offset).rgb * weight;
        result += texture(inputTexture, TexCoords - offset).rgb * weight;
        totalWeight += weight * 2.0;
    }

    result /= totalWeight;

    // Blend between original and blurred based on intensity
    FragColor = vec4(mix(original, result, intensity), 1.0);
}
