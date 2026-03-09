#pragma once
#include "Graphics/PostProcessing/PostProcessEffect.hpp"
#include <memory>
#include <vector>
#include <glm/glm.hpp>

class Shader;

class SSAOEffect : public PostProcessEffect {
public:
	SSAOEffect();
	~SSAOEffect() override;

	bool Initialize() override;
	void Shutdown() override;
	void Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height) override;

	// Returns the blurred SSAO texture for use in tonemapping
	unsigned int GetSSAOTexture() const { return blurTexture; }

	void SetRadius(float r) { radius = r; }
	float GetRadius() const { return radius; }

	void SetBias(float b) { bias = b; }
	float GetBias() const { return bias; }

	void SetIntensity(float i) { ssaoIntensity = i; }
	float GetIntensity() const { return ssaoIntensity; }

	// Must be set each frame before Apply()
	void SetProjectionMatrix(const glm::mat4& proj) { projection = proj; }
	void SetInvProjectionMatrix(const glm::mat4& invProj) { invProjection = invProj; }
	void SetDepthTexture(unsigned int tex) { depthTexture = tex; }

private:
	void CreateFBOs(int halfW, int halfH);
	void DeleteFBOs();
	void GenerateKernel();
	void CreateNoiseTexture();

	std::shared_ptr<Shader> shader;

	// Half-resolution FBOs
	unsigned int ssaoFBO = 0;
	unsigned int ssaoTexture = 0;
	unsigned int blurFBO = 0;
	unsigned int blurTexture = 0;
	int fboWidth = 0;
	int fboHeight = 0;

	// Noise texture (4x4 random vectors)
	unsigned int noiseTextureID = 0;

	// Sample kernel (8 hemisphere samples)
	std::vector<glm::vec3> kernel;

	// Uniforms
	float radius = 0.5f;
	float bias = 0.025f;
	float ssaoIntensity = 1.0f;

	glm::mat4 projection = glm::mat4(1.0f);
	glm::mat4 invProjection = glm::mat4(1.0f);
	unsigned int depthTexture = 0;
};
