#version 300 es
precision mediump float;

in vec2 TexCoord;
in vec4 ParticleColor;

layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BloomEmission;

uniform sampler2D particleTexture;
uniform highp sampler2D particleSceneDepth;
uniform highp vec2 particleDepthRange;
uniform highp float softParticleDistance;

highp float eyeDepth(highp float depth)
{
    highp float nearPlane = particleDepthRange.x;
    highp float farPlane = particleDepthRange.y;
    return nearPlane * farPlane / (farPlane - depth * (farPlane - nearPlane));
}

// Per-entity bloom emission
uniform float bloomIntensity;
uniform vec3 bloomColor;

void main()
{
    vec4 texColor = texture(particleTexture, TexCoord);
    FragColor = texColor * ParticleColor;
    if (softParticleDistance > 0.0) {
        highp vec2 uv = gl_FragCoord.xy / vec2(textureSize(particleSceneDepth, 0));
        highp float sceneDepth = eyeDepth(texture(particleSceneDepth, uv).r);
        highp float fragmentDepth = eyeDepth(gl_FragCoord.z);
        FragColor.a *= smoothstep(0.0, softParticleDistance, sceneDepth - fragmentDepth);
    }

    // Per-entity bloom emission — written only to MRT attachment 1
    BloomEmission = vec4(bloomColor * bloomIntensity, FragColor.a);
}