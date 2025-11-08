#pragma once
#include <string>

class PostProcessEffect {
public:
	PostProcessEffect(const std::string& name) : effectName(name), enabled(true) {};
	virtual ~PostProcessEffect() = default;

	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;
	virtual void Apply(unsigned int inputTexture, unsigned int outputFBO, int width, int height) = 0;
	void SetEnabled(bool enable) { enabled = enable; }
	bool IsEnabled() const { return enabled; }
	const std::string& GetName() const { return effectName; }

protected:
	std::string effectName;
	bool enabled;
};