#pragma once
#include "Graphics/PostProcessing/PostProcessEffect.hpp"
#include <memory>

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

private:
	std::shared_ptr<Shader> shader; 
	float exposure;
	float gamma;
	ToneMappingMode toneMappingMode;
};