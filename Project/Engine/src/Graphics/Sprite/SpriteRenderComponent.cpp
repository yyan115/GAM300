#include "pch.h"
#include "Graphics/Sprite/SpriteRenderComponent.hpp"
#include "Asset Manager/AssetManager.hpp"
#include "Asset Manager/ResourceManager.hpp"
#include "Utilities/GUID.hpp"
#include "Graphics/Texture.h"
#include "Logging.hpp"

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
	if (guidString.empty()) {
		ENGINE_LOG_ERROR("[SpriteRenderComponent] SetTextureFromGUID called with empty GUID string");
		return;
	}

	GUID_128 newTextureGUID = GUIDUtilities::ConvertStringToGUID128(guidString);
	std::string newTexturePath = AssetManager::GetInstance().GetAssetPathFromGUID(newTextureGUID);

	auto loadedTexture = ResourceManager::GetInstance().GetResourceFromGUID<Texture>(newTextureGUID, newTexturePath);
	if (!loadedTexture) {
		ENGINE_LOG_ERROR("[SpriteRenderComponent] SetTextureFromGUID: Failed to load texture from path: " + newTexturePath);
		return;
	}

	// Update the component's texture
	texture = loadedTexture;
	textureGUID = newTextureGUID;
	texturePath = newTexturePath;
}
