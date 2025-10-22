#pragma once

#include <memory>
#include <Asset Manager/AssetMeta.hpp>
#include <functional>

class AssetInspector {
public:
	// GUI rendering method
	static void DrawAssetMetaInfo(std::shared_ptr<AssetMeta> assetMeta, const std::string& assetPath, bool showLockButton = false, bool* isLocked = nullptr, std::function<void()> lockCallback = nullptr);
};