#version 330 core

out float FragColor;
in vec2 TexCoords;

uniform sampler2D depthTexture;
uniform sampler2D noiseTexture;

uniform vec3 samples[8];
uniform mat4 projection;
uniform mat4 invProjection;

uniform vec2 texelSize;       // 1.0 / screenSize (at half res)
uniform vec2 noiseScale;      // screenSize / 4.0 (tile the 4x4 noise)
uniform float radius;
uniform float bias;
uniform float intensity;
uniform int passType;         // 0 = SSAO generation, 1 = blur

// Reconstruct view-space position from depth buffer
vec3 viewPosFromDepth(vec2 uv) {
    float depth = texture(depthTexture, uv).r;
    // Depth buffer to NDC
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    // NDC to view space
    vec4 viewPos = invProjection * ndc;
    return viewPos.xyz / viewPos.w;
}

void main() {
    if (passType == 0) {
        // === SSAO Generation ===
        vec3 fragPos = viewPosFromDepth(TexCoords);

        // Reconstruct normal from depth derivatives
        vec3 dPdx = dFdx(fragPos);
        vec3 dPdy = dFdy(fragPos);
        vec3 normal = normalize(cross(dPdx, dPdy));

        // Random rotation from noise texture (tiled across screen)
        vec3 randomVec = vec3(texture(noiseTexture, TexCoords * noiseScale).rg * 2.0 - 1.0, 0.0);

        // Gram-Schmidt to create TBN basis oriented along the surface normal
        vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
        vec3 bitangent = cross(normal, tangent);
        mat3 TBN = mat3(tangent, bitangent, normal);

        float occlusion = 0.0;
        for (int i = 0; i < 8; ++i) {
            // Sample position in view space
            vec3 samplePos = fragPos + TBN * samples[i] * radius;

            // Project sample to screen space
            vec4 offset = projection * vec4(samplePos, 1.0);
            offset.xyz /= offset.w;
            offset.xyz = offset.xyz * 0.5 + 0.5;

            // Sample depth at projected position
            float sampleDepth = viewPosFromDepth(offset.xy).z;

            // Range check: only occlude if sample is within radius
            float rangeCheck = smoothstep(0.0, 1.0, radius / abs(fragPos.z - sampleDepth));
            occlusion += (sampleDepth >= samplePos.z + bias ? 1.0 : 0.0) * rangeCheck;
        }

        occlusion = 1.0 - (occlusion / 8.0) * intensity;
        FragColor = occlusion;

    } else {
        // === Blur Pass (simple 4x4 box blur) ===
        float result = 0.0;
        for (int x = -2; x <= 2; ++x) {
            for (int y = -2; y <= 2; ++y) {
                result += texture(depthTexture, TexCoords + vec2(float(x), float(y)) * texelSize).r;
            }
        }
        FragColor = result / 25.0;
    }
}
