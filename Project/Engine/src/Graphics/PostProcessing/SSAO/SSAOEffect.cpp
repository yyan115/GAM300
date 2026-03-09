#include "pch.h"
#include "Graphics/PostProcessing/SSAO/SSAOEffect.hpp"
#include "Graphics/PostProcessing/PostProcessingManager.hpp"
#include "Logging.hpp"
#include <Asset Manager/ResourceManager.hpp>
#include <random>

SSAOEffect::SSAOEffect()
	: PostProcessEffect("SSAO")
{
}

SSAOEffect::~SSAOEffect()
{
	Shutdown();
}

bool SSAOEffect::Initialize()
{
	ENGINE_PRINT("[SSAOEffect] Initializing...\n");

	std::string shaderPath = ResourceManager::GetPlatformShaderPath("ssao");
	GUID_128 shaderGUID = MetaFilesManager::GetGUID128FromAssetFile(shaderPath);
	shader = ResourceManager::GetInstance().GetResourceFromGUID<Shader>(shaderGUID, shaderPath);

	if (!shader) {
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SSAOEffect] Failed to load SSAO shader from path: ", shaderPath, "\n");
		return false;
	}

	GenerateKernel();
	CreateNoiseTexture();

	ENGINE_PRINT("[SSAOEffect] Initialized successfully\n");
	return true;
}

void SSAOEffect::Shutdown()
{
	DeleteFBOs();

	if (noiseTextureID != 0) {
		glDeleteTextures(1, &noiseTextureID);
		noiseTextureID = 0;
	}

	ENGINE_PRINT("[SSAOEffect] Shutdown complete\n");
}

void SSAOEffect::GenerateKernel()
{
	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	std::default_random_engine generator(42); // Fixed seed for determinism

	kernel.clear();
	for (int i = 0; i < 8; ++i) {
		glm::vec3 sample(
			randomFloats(generator) * 2.0f - 1.0f,
			randomFloats(generator) * 2.0f - 1.0f,
			randomFloats(generator) // Hemisphere: z always positive
		);
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);

		// Accelerating interpolation: distribute more samples closer to origin
		float scale = static_cast<float>(i) / 8.0f;
		scale = 0.1f + scale * scale * 0.9f; // lerp(0.1, 1.0, scale*scale)
		sample *= scale;

		kernel.push_back(sample);
	}
}

void SSAOEffect::CreateNoiseTexture()
{
	std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);
	std::default_random_engine generator(123);

	// 4x4 noise texture (random rotation vectors in tangent space)
	std::vector<glm::vec3> noise;
	for (int i = 0; i < 16; ++i) {
		glm::vec3 n(
			randomFloats(generator) * 2.0f - 1.0f,
			randomFloats(generator) * 2.0f - 1.0f,
			0.0f // Rotate around z-axis only
		);
		noise.push_back(n);
	}

	glGenTextures(1, &noiseTextureID);
	glBindTexture(GL_TEXTURE_2D, noiseTextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, 4, 4, 0, GL_RGB, GL_FLOAT, noise.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void SSAOEffect::CreateFBOs(int halfW, int halfH)
{
	DeleteFBOs();
	fboWidth = halfW;
	fboHeight = halfH;

	// SSAO FBO (single channel, half res)
	glGenFramebuffers(1, &ssaoFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

	glGenTextures(1, &ssaoTexture);
	glBindTexture(GL_TEXTURE_2D, ssaoTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, halfW, halfH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SSAOEffect] SSAO FBO not complete!\n");

	// Blur FBO (single channel, half res)
	glGenFramebuffers(1, &blurFBO);
	glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);

	glGenTextures(1, &blurTexture);
	glBindTexture(GL_TEXTURE_2D, blurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, halfW, halfH, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, blurTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[SSAOEffect] Blur FBO not complete!\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAOEffect::DeleteFBOs()
{
	if (ssaoTexture) { glDeleteTextures(1, &ssaoTexture); ssaoTexture = 0; }
	if (ssaoFBO) { glDeleteFramebuffers(1, &ssaoFBO); ssaoFBO = 0; }
	if (blurTexture) { glDeleteTextures(1, &blurTexture); blurTexture = 0; }
	if (blurFBO) { glDeleteFramebuffers(1, &blurFBO); blurFBO = 0; }
	fboWidth = 0;
	fboHeight = 0;
}

void SSAOEffect::Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height)
{
	if (!shader || !enabled) return;

	int halfW = width / 2;
	int halfH = height / 2;
	if (halfW < 1) halfW = 1;
	if (halfH < 1) halfH = 1;

	// Recreate FBOs if size changed
	if (ssaoFBO == 0 || fboWidth != halfW || fboHeight != halfH)
		CreateFBOs(halfW, halfH);

	glDisable(GL_DEPTH_TEST);

	// === Pass 0: SSAO Generation ===
	glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
	glViewport(0, 0, halfW, halfH);
	glClear(GL_COLOR_BUFFER_BIT);

	shader->Activate();
	shader->setInt("passType", 0);
	shader->setInt("depthTexture", 0);
	shader->setInt("noiseTexture", 1);
	shader->setMat4("projection", projection);
	shader->setMat4("invProjection", invProjection);
	shader->setVec2("texelSize", glm::vec2(1.0f / halfW, 1.0f / halfH));
	shader->setVec2("noiseScale", glm::vec2(halfW / 4.0f, halfH / 4.0f));
	shader->setFloat("radius", radius);
	shader->setFloat("bias", bias);
	shader->setFloat("intensity", ssaoIntensity);

	for (int i = 0; i < 8; ++i) {
		shader->setVec3("samples[" + std::to_string(i) + "]", kernel[i]);
	}

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, noiseTextureID);

	PostProcessingManager::GetInstance().RenderScreenQuad();

	// === Pass 1: Blur ===
	glBindFramebuffer(GL_FRAMEBUFFER, blurFBO);
	glViewport(0, 0, halfW, halfH);
	glClear(GL_COLOR_BUFFER_BIT);

	shader->Activate();
	shader->setInt("passType", 1);
	shader->setInt("depthTexture", 0); // Reuse depthTexture uniform for SSAO input
	shader->setVec2("texelSize", glm::vec2(1.0f / halfW, 1.0f / halfH));

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ssaoTexture);

	PostProcessingManager::GetInstance().RenderScreenQuad();

	// Cleanup
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	glEnable(GL_DEPTH_TEST);
}
