#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D inputTexture;
uniform int passType;       // 0=extract, 1=h-blur, 2=v-blur, 3=composite
uniform float threshold;
uniform float intensity;
uniform vec2 texelSize;

// Gaussian weights for blur
const float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main()
{
    if (passType == 0)
    {
        // Brightness extraction: keep only bright pixels
        vec3 color = texture(inputTexture, TexCoords).rgb;
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > threshold)
            FragColor = vec4(color, 1.0);
        else
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else if (passType == 1)
    {
        // Horizontal Gaussian blur
        vec3 result = texture(inputTexture, TexCoords).rgb * weights[0];
        for (int i = 1; i < 5; ++i)
        {
            vec2 offset = vec2(texelSize.x * float(i) * 2.0, 0.0);
            result += texture(inputTexture, TexCoords + offset).rgb * weights[i];
            result += texture(inputTexture, TexCoords - offset).rgb * weights[i];
        }
        FragColor = vec4(result, 1.0);
    }
    else if (passType == 2)
    {
        // Vertical Gaussian blur
        vec3 result = texture(inputTexture, TexCoords).rgb * weights[0];
        for (int i = 1; i < 5; ++i)
        {
            vec2 offset = vec2(0.0, texelSize.y * float(i) * 2.0);
            result += texture(inputTexture, TexCoords + offset).rgb * weights[i];
            result += texture(inputTexture, TexCoords - offset).rgb * weights[i];
        }
        FragColor = vec4(result, 1.0);
    }
    else if (passType == 3)
    {
        // Composite: output blurred bloom texture scaled by intensity
        vec3 bloom = texture(inputTexture, TexCoords).rgb;
        FragColor = vec4(bloom * intensity, 1.0);
    }
}
