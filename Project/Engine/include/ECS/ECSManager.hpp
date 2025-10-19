#pragma once

#include <memory>
#include <vector>

#include "EntityManager.hpp"
#include "ComponentManager.hpp"
#include "SystemManager.hpp"
#include <Transform/TransformSystem.hpp>
#include <Graphics/Model/ModelSystem.hpp>
#include <Graphics/TextRendering/TextRenderingSystem.hpp>
#include <Graphics/DebugDraw/DebugDrawSystem.hpp>
#include "../Engine.h"  // For ENGINE_API macro
#include <Graphics/Lights/LightingSystem.hpp>
#include <Graphics/Sprite/SpriteSystem.hpp>
#include <Graphics/Particle/ParticleSystem.hpp>
#include <Sound/AudioSystem.hpp>
#include "Graphics/Camera/CameraSystem.hpp"

class PhysicsSystem;
class ECSManager {
public:
	ECSManager() { Initialize(); };
	~ECSManager() {};

	void ENGINE_API Initialize();

	Entity ENGINE_API CreateEntity();
	Entity CreateEntityWithGUID(const GUID_128& guid);

	void ENGINE_API DestroyEntity(Entity entity);

	void ClearAllEntities();

	template <typename T>
	void RegisterComponent() {
		componentManager->RegisterComponent<T>();
	}

	template<typename T>
	bool IsComponentTypeRegistered() const
	{
		return componentManager                     // unique_ptr exists?
			&& componentManager->IsRegistered<T>(); // note the -> here
	}

	template <typename T>
	void AddComponent(Entity entity, T component) {
		// Add the component to the entity via the ComponentManager.
		componentManager->AddComponent<T>(entity, component);

		// Update the entity's signature via the EntityManager.
		auto signature = entityManager->GetEntitySignature(entity);
		signature.set(componentManager->GetComponentID<T>(), true);
		entityManager->SetEntitySignature(entity, signature);

		// Notify the SystemManager of the entity's signature change.
		systemManager->OnEntitySignatureChanged(entity, signature);
	}

	template <typename T>
	void RemoveComponent(Entity entity) {
		// Remove the component from the entity via the ComponentManager.
		componentManager->RemoveComponent<T>(entity);

		// Update the entity's signature via the EntityManager.
		auto signature = entityManager->GetEntitySignature(entity);
		signature.set(componentManager->GetComponentID<T>(), false);
		entityManager->SetEntitySignature(entity, signature);

		// Notify the SystemManager of the entity's signature change.
		systemManager->OnEntitySignatureChanged(entity, signature);
	}

	template <typename T>
	T& GetComponent(Entity entity) {
		return componentManager->GetComponent<T>(entity);
	}

	template <typename T>
	std::optional<std::reference_wrapper<T>> TryGetComponent(Entity entity) {
		return componentManager->TryGetComponent<T>(entity);
	}

	template <typename T>
	bool HasComponent(Entity entity) {
		return TryGetComponent<T>(entity).has_value();
	}

	template <typename T>
	std::shared_ptr<T> RegisterSystem() {
		return systemManager->RegisterSystem<T>();
	}

	template <typename T>
	void SetSystemSignature(Signature signature) {
		systemManager->SetSignature<T>(signature);
	}

	std::vector<Entity> GetActiveEntities() const {
		return entityManager->GetActiveEntities();
	}

	std::vector<Entity> GetAllEntities() const {
		return entityManager->GetAllEntities();
	}
	
	// Get system manager for profiling access
	const SystemManager* GetSystemManager() const {
		return systemManager.get();
	}

	// STORE SHARED POINTERS TO SYSTEMS HERE
	// e.g., 
	std::shared_ptr<TransformSystem> transformSystem;
	std::shared_ptr<ModelSystem> modelSystem;
	std::shared_ptr<TextRenderingSystem> textSystem;
	std::shared_ptr<DebugDrawSystem> debugDrawSystem;
	std::shared_ptr<PhysicsSystem> physicsSystem;
	std::shared_ptr<LightingSystem> lightingSystem;
	std::shared_ptr<SpriteSystem> spriteSystem;
	std::shared_ptr<ParticleSystem> particleSystem;
	std::shared_ptr<AudioSystem> audioSystem;
	std::shared_ptr<AnimationSystem> animationSystem;
	std::shared_ptr<CameraSystem> cameraSystem;

private:
	template <typename T>
	ComponentID GetComponentID() {
		return componentManager->GetComponentID<T>();
	}

	std::unique_ptr<EntityManager> entityManager;
	std::unique_ptr<ComponentManager> componentManager;
	std::unique_ptr<SystemManager> systemManager;
};