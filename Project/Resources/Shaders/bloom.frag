#version 330 core

out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D inputTexture;
uniform int passType;       // 0=extract, 1=downsample, 2=upsample, 3=composite
uniform float threshold;
uniform float intensity;
uniform float scatter;      // upsample blend weight (0..1), controls bloom spread energy
uniform vec2 texelSize;

void main()
{
    if (passType == 0)
    {
        // Brightness extraction: keep only bright pixels (fallback when no MRT)
        vec3 color = texture(inputTexture, TexCoords).rgb;
        float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
        if (brightness > threshold)
            FragColor = vec4(color, 1.0);
        else
            FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
    else if (passType == 1)
    {
        // 13-tap downsample (Jimenez 2014 / Unity bloom)
        // Reduces firefly artifacts compared to simple bilinear downsampling
        vec2 uv = TexCoords;

        // Center sample
        vec3 a = texture(inputTexture, uv).rgb;

        // Inner 4 samples (diamond at ±1 texel)
        vec3 b = texture(inputTexture, uv + vec2(-1.0, -1.0) * texelSize).rgb;
        vec3 c = texture(inputTexture, uv + vec2( 1.0, -1.0) * texelSize).rgb;
        vec3 d = texture(inputTexture, uv + vec2(-1.0,  1.0) * texelSize).rgb;
        vec3 e = texture(inputTexture, uv + vec2( 1.0,  1.0) * texelSize).rgb;

        // Outer 8 samples (at ±2 texels)
        vec3 f = texture(inputTexture, uv + vec2(-2.0, -2.0) * texelSize).rgb;
        vec3 g = texture(inputTexture, uv + vec2( 0.0, -2.0) * texelSize).rgb;
        vec3 h = texture(inputTexture, uv + vec2( 2.0, -2.0) * texelSize).rgb;
        vec3 i = texture(inputTexture, uv + vec2(-2.0,  0.0) * texelSize).rgb;
        vec3 j = texture(inputTexture, uv + vec2( 2.0,  0.0) * texelSize).rgb;
        vec3 k = texture(inputTexture, uv + vec2(-2.0,  2.0) * texelSize).rgb;
        vec3 l = texture(inputTexture, uv + vec2( 0.0,  2.0) * texelSize).rgb;
        vec3 m = texture(inputTexture, uv + vec2( 2.0,  2.0) * texelSize).rgb;

        // Weighted average (Jimenez weights)
        vec3 result = a * 0.125;                               // center: 1/8
        result += (b + c + d + e) * 0.125;                     // inner 4: 4 * 1/8 = 1/2
        result += (f + h + k + m) * 0.03125;                   // corners: 4 * 1/32 = 1/8
        result += (g + i + j + l) * 0.0625;                    // edges: 4 * 1/16 = 1/4

        FragColor = vec4(result, 1.0);
    }
    else if (passType == 2)
    {
        // 9-tap tent filter upsample (3x3 with tent weights)
        // Produces smooth, artifact-free upsampling
        vec2 uv = TexCoords;

        vec3 a = texture(inputTexture, uv + vec2(-1.0, -1.0) * texelSize).rgb;
        vec3 b = texture(inputTexture, uv + vec2( 0.0, -1.0) * texelSize).rgb;
        vec3 c = texture(inputTexture, uv + vec2( 1.0, -1.0) * texelSize).rgb;
        vec3 d = texture(inputTexture, uv + vec2(-1.0,  0.0) * texelSize).rgb;
        vec3 e = texture(inputTexture, uv + vec2( 0.0,  0.0) * texelSize).rgb;
        vec3 f = texture(inputTexture, uv + vec2( 1.0,  0.0) * texelSize).rgb;
        vec3 g = texture(inputTexture, uv + vec2(-1.0,  1.0) * texelSize).rgb;
        vec3 h = texture(inputTexture, uv + vec2( 0.0,  1.0) * texelSize).rgb;
        vec3 i = texture(inputTexture, uv + vec2( 1.0,  1.0) * texelSize).rgb;

        // Tent filter weights: (1,2,1 / 2,4,2 / 1,2,1) / 16
        vec3 result = (a + c + g + i) * (1.0 / 16.0);         // corners
        result += (b + d + f + h) * (2.0 / 16.0);              // edges
        result += e * (4.0 / 16.0);                             // center

        // Scale by scatter to control energy accumulation across mip levels
        FragColor = vec4(result * scatter, 1.0);
    }
    else if (passType == 3)
    {
        // Composite: output blurred bloom texture scaled by intensity
        vec3 bloom = texture(inputTexture, TexCoords).rgb;
        FragColor = vec4(bloom * intensity, 1.0);
    }
}
