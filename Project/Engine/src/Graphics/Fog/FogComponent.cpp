#include "pch.h"
#include "Graphics/Fog/FogComponent.hpp"
#include "Reflection/ReflectionBase.hpp"

template<>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<FogShape>() {
    using U = std::underlying_type_t<FogShape>;
    return GetPrimitiveDescriptor<U>();
}

#pragma region Reflection
REFL_REGISTER_START(FogVolumeComponent)
	REFL_REGISTER_PROPERTY(shape)
	REFL_REGISTER_PROPERTY(fogColor)
	REFL_REGISTER_PROPERTY(fogColorAlpha)
	REFL_REGISTER_PROPERTY(density)
	REFL_REGISTER_PROPERTY(opacity)
	REFL_REGISTER_PROPERTY(scrollSpeedX)
	REFL_REGISTER_PROPERTY(scrollSpeedY)
	REFL_REGISTER_PROPERTY(noiseScale)
	REFL_REGISTER_PROPERTY(noiseStrength)
	REFL_REGISTER_PROPERTY(warpStrength)
	REFL_REGISTER_PROPERTY(useHeightFade)
	REFL_REGISTER_PROPERTY(heightFadeStart)
	REFL_REGISTER_PROPERTY(heightFadeEnd)
	REFL_REGISTER_PROPERTY(edgeSoftness)
	REFL_REGISTER_PROPERTY(noiseTextureGUID)
REFL_REGISTER_END;
#pragma endregion