#pragma once
#include "Graphics/PostProcessing/PostProcessEffect.hpp"
#include <memory>
#include <glm/glm.hpp>

class Shader;

class HDREffect : public PostProcessEffect {
public:
	enum class ToneMappingMode {
		REINHARD,       // Simple Reinhard tone mapping
		EXPOSURE,       // Exposure-based tone mapping
		ACES_FILMIC     // Cinematic ACES tone mapping
	};

	HDREffect();
	~HDREffect() override;

	bool Initialize() override;
	void Shutdown() override;
	void Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height) override;

	// HDR-specific parameters
	void SetExposure(float exp) { exposure = exp; }
	float GetExposure() const { return exposure; }

	void SetGamma(float g) { gamma = g; }
	float GetGamma() const { return gamma; }

	void SetToneMappingMode(ToneMappingMode mode) { toneMappingMode = mode; }
	ToneMappingMode GetToneMappingMode() const { return toneMappingMode; }

	// Vignette (applied in tonemapping shader)
	void SetVignetteEnabled(bool e) { vignetteEnabled = e; }
	void SetVignetteIntensity(float i) { vignetteIntensity = i; }
	void SetVignetteSmoothness(float s) { vignetteSmoothness = s; }
	void SetVignetteColor(const glm::vec3& c) { vignetteColor = c; }

	// Color Grading (applied in tonemapping shader)
	void SetColorGradingEnabled(bool e) { colorGradingEnabled = e; }
	void SetCGBrightness(float b) { cgBrightness = b; }
	void SetCGContrast(float c) { cgContrast = c; }
	void SetCGSaturation(float s) { cgSaturation = s; }
	void SetCGTint(const glm::vec3& t) { cgTint = t; }

	// Chromatic Aberration (applied in tonemapping shader)
	void SetChromaticAberrationEnabled(bool e) { caEnabled = e; }
	void SetChromaticAberrationIntensity(float i) { caIntensity = i; }
	void SetChromaticAberrationPadding(float p) { caPadding = p; }

	// SSAO (applied in tonemapping shader)
	void SetSSAOEnabled(bool e) { ssaoEnabled = e; }
	void SetSSAOTexture(unsigned int tex) { ssaoTexture = tex; }

private:
	std::shared_ptr<Shader> shader;
	float exposure;
	float gamma;
	ToneMappingMode toneMappingMode;

	// Vignette
	bool vignetteEnabled = false;
	float vignetteIntensity = 0.5f;
	float vignetteSmoothness = 0.5f;
	glm::vec3 vignetteColor = glm::vec3(0.0f);

	// Color Grading
	bool colorGradingEnabled = false;
	float cgBrightness = 0.0f;
	float cgContrast = 1.0f;
	float cgSaturation = 1.0f;
	glm::vec3 cgTint = glm::vec3(1.0f);

	// Chromatic Aberration
	bool caEnabled = false;
	float caIntensity = 0.5f;
	float caPadding = 0.5f;

	// SSAO
	bool ssaoEnabled = false;
	unsigned int ssaoTexture = 0;
};