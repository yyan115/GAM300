#include "pch.h"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Utilities/GUID.hpp"
#include "Graphics/Texture.h"

#pragma region Reflection
REFL_REGISTER_START(SpriteRenderComponent)
	REFL_REGISTER_PROPERTY(isVisible)
	REFL_REGISTER_PROPERTY(textureGUID)
	REFL_REGISTER_PROPERTY(shaderGUID)
	REFL_REGISTER_PROPERTY(position)
	REFL_REGISTER_PROPERTY(scale)
	REFL_REGISTER_PROPERTY(rotation)
	REFL_REGISTER_PROPERTY(color)
	REFL_REGISTER_PROPERTY(alpha)
	REFL_REGISTER_PROPERTY(is3D)
	REFL_REGISTER_PROPERTY(enableBillboard)
	REFL_REGISTER_PROPERTY(sortingLayer)
	REFL_REGISTER_PROPERTY(sortingOrder)
	REFL_REGISTER_PROPERTY(saved3DPosition)
REFL_REGISTER_END
#pragma endregion

void SpriteRenderComponent::SetTextureFromGUID(const std::string& guidString) {
	GUID_128 textureGUID = GUIDUtilities::ConvertStringToGUID128(guidString);
	std::string texturePath = AssetManager::GetInstance().GetAssetPathFromGUID(textureGUID);
	texture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(textureGUID, texturePath);
	this->textureGUID = textureGUID;
	this->texturePath = texturePath;
}
