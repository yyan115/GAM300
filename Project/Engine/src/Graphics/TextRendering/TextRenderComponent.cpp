#include "pch.h"
#include "Graphics/TextRendering/TextRenderComponent.hpp"

#pragma region Reflection
REFL_REGISTER_START(TextRenderComponent)
	REFL_REGISTER_PROPERTY(text)
	REFL_REGISTER_PROPERTY(fontSize)
	REFL_REGISTER_PROPERTY(fontGUID)
	REFL_REGISTER_PROPERTY(shaderGUID)
	REFL_REGISTER_PROPERTY(position)
	REFL_REGISTER_PROPERTY(color)
	REFL_REGISTER_PROPERTY(scale)
	REFL_REGISTER_PROPERTY(is3D)
	REFL_REGISTER_PROPERTY(transform)
	REFL_REGISTER_PROPERTY(alignmentInt)
REFL_REGISTER_END;
#pragma endregion