#include "pch.h"
#include "UI/Button/ButtonSystem.hpp"
#include <Performance/PerformanceProfiler.hpp>
#include <ECS/ECSRegistry.hpp>
#include <Graphics/GraphicsManager.hpp>
#include <Input/InputManager.hpp>

void ButtonSystem::Initialise() {
	ENGINE_LOG_INFO("ButtonSystem Initialized");
}

void ButtonSystem::Update() {
	PROFILE_FUNCTION();
	
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	GraphicsManager& gfxManager = GraphicsManager::GetInstance();

	// Check if rendering for editor.
	bool isRenderingForEditor = gfxManager.IsRenderingForEditor();

	if (InputManager::GetMouseButtonDown(Input::MouseButton::LEFT)) {
		for (const auto& entity : entities) {
			HandleMouseClick(entity, Vector3D(InputManager::GetMouseX(), InputManager::GetMouseY(), 0.0f));
		}
	}
}

void ButtonSystem::OnClickAddListener(ButtonComponent& button, lua_State* L, int funcIndex) {
	button.onClick.AddListener(L, funcIndex);
}

void ButtonSystem::HandleMouseClick(Entity buttonEntity, Vector3D mousePos) {
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	if (ecsManager.HasComponent<SpriteRenderComponent>(buttonEntity)) {
		auto& spriteComponent = ecsManager.GetComponent<SpriteRenderComponent>(buttonEntity);
		if (spriteComponent.is3D) {
			// Ignore 3D buttons for now
			ENGINE_LOG_WARN("[ButtonSystem] 3D buttons not supported yet.");
			return;
		}

		auto& transform = ecsManager.GetComponent<Transform>(buttonEntity);
		float halfExtentsX = transform.localScale.x / 2.0f;
		float halfExtentsY = transform.localScale.y / 2.0f;
		float minX = transform.localPosition.x - halfExtentsX;
		float maxX = transform.localPosition.x + halfExtentsX;
		float minY = transform.localPosition.y - halfExtentsY;
		float maxY = transform.localPosition.y + halfExtentsY;

		if (mousePos.x >= minX && mousePos.x <= maxX && mousePos.y >= minY && mousePos.y <= maxY) {
			auto& buttonComponent = ecsManager.GetComponent<ButtonComponent>(buttonEntity);
			buttonComponent.onClick.Invoke();
		}
	}
}
