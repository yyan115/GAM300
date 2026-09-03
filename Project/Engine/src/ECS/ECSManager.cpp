#include "pch.h"
#include "ECS/ECSRegistry.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "ECS/NameComponent.hpp"
#include "ECS/ActiveComponent.hpp"
#include <Transform/TransformComponent.hpp>
#include <Math/Vector3D.hpp>
#include <Graphics/Model/ModelSystem.hpp>
#include <Graphics/Model/ModelRenderComponent.hpp>
#include <Graphics/TextRendering/TextRenderComponent.hpp>
#include "ECS/NameComponent.hpp"
#include <Graphics/Lights/LightComponent.hpp>
#include <Hierarchy/ParentComponent.hpp>
#include <Hierarchy/ChildrenComponent.hpp>
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioListenerComponent.hpp"
#include "Sound/AudioReverbZoneComponent.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Prefab/PrefabLinkComponent.hpp"
#include "Logging.hpp"
#include "Hierarchy/EntityGUIDRegistry.hpp"
#include "Game AI/BrainComponent.hpp"

#include <Physics/ColliderComponent.hpp>
#include <Physics/RigidBodyComponent.hpp>
#include "ECS/TagComponent.hpp"
#include "ECS/LayerComponent.hpp"
#include <Physics/PhysicsSystem.hpp>
#include <Physics/Kinematics/CharacterControllerSystem.hpp>

#include "Script/ScriptComponentData.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include <ECS/TagComponent.hpp>
#include <ECS/LayerComponent.hpp>
#include <ECS/SiblingIndexComponent.hpp>
#include "UI/Button/ButtonComponent.hpp"
#include "UI/Slider/SliderComponent.hpp"
#include "UI/Slider/SliderSystem.hpp"
#include "UI/Anchor/UIAnchorComponent.hpp"
#include "UI/Anchor/UIAnchorSystem.hpp"
#include <Graphics/Sprite/SpriteAnimationComponent.hpp>
#include "Video/VideoComponent.hpp"
#include "Video/VideoSystem.hpp"
#include "Dialogue/DialogueComponent.hpp"
#include "Graphics/BloomComponent.hpp"
#include "Dialogue/DialogueSystem.hpp"

namespace {
	constexpr std::uint64_t kActiveHierarchyStateBit = 1;
	constexpr std::uint64_t kActiveHierarchyEpochMask = ~kActiveHierarchyStateBit;

	bool IsActiveHierarchyCacheHit(std::uint64_t cachedValue, std::uint64_t epoch) {
		return (cachedValue & kActiveHierarchyEpochMask) == epoch;
	}

	bool CachedActiveHierarchyState(std::uint64_t cachedValue) {
		return (cachedValue & kActiveHierarchyStateBit) != 0;
	}

	std::uint64_t PackActiveHierarchyState(std::uint64_t epoch, bool isActive) {
		return epoch | (isActive ? kActiveHierarchyStateBit : 0);
	}
}

void ECSManager::Initialize() {
	entityManager = std::make_unique<EntityManager>();
	componentManager = std::make_unique<ComponentManager>();
	systemManager = std::make_unique<SystemManager>();

	// REGISTER ALL COMPONENTS HERE
	// e.g., 
	RegisterComponent<Transform>();
	RegisterComponent<ModelRenderComponent>();
	RegisterComponent<TextRenderComponent>();
	RegisterComponent<DebugDrawComponent>();
	RegisterComponent<NameComponent>();
	RegisterComponent<ActiveComponent>();
	RegisterComponent<ColliderComponent>();
	RegisterComponent<RigidBodyComponent>();
	RegisterComponent<VideoComponent>();
	RegisterComponent<LightComponent>();
	RegisterComponent<DirectionalLightComponent>();
	RegisterComponent<PointLightComponent>();
	RegisterComponent<SpotLightComponent>();
	RegisterComponent<ParentComponent>();
	RegisterComponent<ChildrenComponent>();
	RegisterComponent<AudioComponent>();
	RegisterComponent<AudioListenerComponent>();
	RegisterComponent<AudioReverbZoneComponent>();
	RegisterComponent<SpriteRenderComponent>();
	RegisterComponent<ParticleComponent>();
	RegisterComponent<AnimationComponent>();
	RegisterComponent<PrefabLinkComponent>();
	RegisterComponent<CameraComponent>();
	RegisterComponent<TagComponent>();
	RegisterComponent<LayerComponent>();
	RegisterComponent<SiblingIndexComponent>();
	RegisterComponent<ScriptComponentData>();
	RegisterComponent<BrainComponent>();
	RegisterComponent<SpriteAnimationComponent>();
	RegisterComponent<ButtonComponent>();
	RegisterComponent<SliderComponent>();
	RegisterComponent<UIAnchorComponent>();
	RegisterComponent<DialogueComponent>();
	RegisterComponent<FogVolumeComponent>();
	RegisterComponent<BloomComponent>();

	// REGISTER ALL SYSTEMS AND ITS SIGNATURES HERE
	// e.g.,
	transformSystem = RegisterSystem<TransformSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<Transform>());
		SetSystemSignature<TransformSystem>(signature);
	}

	modelSystem = RegisterSystem<ModelSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ModelRenderComponent>());
		SetSystemSignature<ModelSystem>(signature);
	}

	textSystem = RegisterSystem<TextRenderingSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<TextRenderComponent>());
		SetSystemSignature<TextRenderingSystem>(signature);
	}

	debugDrawSystem = RegisterSystem<DebugDrawSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<DebugDrawComponent>());
		SetSystemSignature<DebugDrawSystem>(signature);
	}

	physicsSystem = RegisterSystem<PhysicsSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ColliderComponent>());
		signature.set(GetComponentID<RigidBodyComponent>());
		SetSystemSignature<PhysicsSystem>(signature);
	}
 
	characterControllerSystem = RegisterSystem<CharacterControllerSystem>();
	{
		Signature signature;

		signature.set(GetComponentID<ColliderComponent>());
		signature.set(GetComponentID<RigidBodyComponent>());
		SetSystemSignature<CharacterControllerSystem>(signature);
	}

	if (physicsSystem && characterControllerSystem)
	{
		JPH::PhysicsSystem* joltPhysics = &physicsSystem->GetJoltSystem();
		if (joltPhysics)
		{
			characterControllerSystem->SetPhysicsSystem(joltPhysics);
		}
	}


	lightingSystem = RegisterSystem<LightingSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<DirectionalLightComponent>());
		signature.set(GetComponentID<PointLightComponent>());
		signature.set(GetComponentID<SpotLightComponent>());
		SetSystemSignature<LightingSystem>(signature);
	}

	// UIAnchorSystem must run BEFORE sprite/button/text systems
	// so that Transform positions are updated before rendering
	uiAnchorSystem = RegisterSystem<UIAnchorSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<UIAnchorComponent>());
		SetSystemSignature<UIAnchorSystem>(signature);
	}

	spriteSystem = RegisterSystem<SpriteSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<SpriteRenderComponent>());
		SetSystemSignature<SpriteSystem>(signature);
	}

	particleSystem = RegisterSystem<ParticleSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ParticleComponent>());
		SetSystemSignature<ParticleSystem>(signature);
	}
	
	audioSystem = RegisterSystem<AudioSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<AudioComponent>());
		signature.set(GetComponentID<AudioListenerComponent>());
		signature.set(GetComponentID<AudioReverbZoneComponent>());
		SetSystemSignature<AudioSystem>(signature);
	}

	animationSystem = RegisterSystem<AnimationSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<AnimationComponent>());
		SetSystemSignature<AnimationSystem>(signature);
	}
	
	cameraSystem = RegisterSystem<CameraSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<CameraComponent>()); 
		SetSystemSignature<CameraSystem>(signature); 
	}

	scriptSystem = RegisterSystem<ScriptSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ScriptComponentData>());
		SetSystemSignature<ScriptSystem>(signature);
	}

	spriteAnimationSystem = RegisterSystem<SpriteAnimationSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<SpriteAnimationComponent>());
		SetSystemSignature<SpriteAnimationSystem>(signature);
	}

	buttonSystem = RegisterSystem<ButtonSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<ButtonComponent>());
		SetSystemSignature<ButtonSystem>(signature);
	}

	sliderSystem = RegisterSystem<SliderSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<SliderComponent>());
		SetSystemSignature<SliderSystem>(signature);
	}

	videoSystem = RegisterSystem<VideoSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<VideoComponent>());
		SetSystemSignature<VideoSystem>(signature);
	}

	dialogueSystem = RegisterSystem<DialogueSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<DialogueComponent>());
		SetSystemSignature<DialogueSystem>(signature);
	}

	fogSystem = RegisterSystem<FogSystem>();
	{
		Signature signature;
		signature.set(GetComponentID<FogVolumeComponent>());
		SetSystemSignature<FogSystem>(signature);
	}

}

Entity ECSManager::CreateEntity() {
	// Register the entity with a new GUID
	GUID_string guidStr = GUIDUtilities::GenerateGUIDString();
	GUID_128 guid = GUIDUtilities::ConvertStringToGUID128(guidStr);

	return CreateEntityWithGUID(guid);
}

Entity ECSManager::CreateEntityWithGUID(const GUID_128& guid) {
	Entity entity = entityManager->CreateEntity();
	EntityGUIDRegistry::GetInstance().Register(entity, guid);
	//ENGINE_PRINT("[ECSManager] Created entity ", entity, ". Total active entities: ", entityManager->GetActiveEntityCount(), "\n");

	// Add default components here (e.g. Name, Transform, etc.)
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	ecsManager.AddComponent<NameComponent>(entity, NameComponent("Entity_" + std::to_string(entity)));
	ecsManager.AddComponent<ActiveComponent>(entity, ActiveComponent(true)); // Entity active by default

	Transform defaultTransform;
	defaultTransform.localPosition = Vector3D(0.0f, 0.0f, 0.0f);
	defaultTransform.localScale = Vector3D(1.0f, 1.0f, 1.0f);
	defaultTransform.localRotation = Quaternion();
	defaultTransform.isDirty = true;

	ecsManager.AddComponent<Transform>(entity, defaultTransform);

	// Add default tag and layer components
	ecsManager.AddComponent<TagComponent>(entity, TagComponent(0)); // Default to first tag
	ecsManager.AddComponent<LayerComponent>(entity, LayerComponent(0)); // Default to first layer

	return entity;
}

void ECSManager::DestroyEntity(Entity entity) {
	// Recursively destroy children first to avoid dangling ParentComponent references
	if (HasComponent<ChildrenComponent>(entity)) {
		auto childrenCopy = GetComponent<ChildrenComponent>(entity).children;
		for (const auto& childGuid : childrenCopy) {
			Entity child = EntityGUIDRegistry::GetInstance().GetEntityByGUID(childGuid);
			if (child != static_cast<Entity>(-1) && child != entity) {
				DestroyEntity(child);
			}
		}
	}

	// Remove the destroyed entity from its parent's children component, if it was a child.
	if (HasComponent<ParentComponent>(entity)) {
		GUID_128 entityGUID = EntityGUIDRegistry::GetInstance().GetGUIDByEntity(entity);
		Entity parent = EntityGUIDRegistry::GetInstance().GetEntityByGUID(GetComponent<ParentComponent>(entity).parent);
		if (parent != static_cast<Entity>(-1) && HasComponent<ChildrenComponent>(parent)) {
			auto& childrenComp = GetComponent<ChildrenComponent>(parent);
			auto it = std::find(childrenComp.children.begin(), childrenComp.children.end(), entityGUID);
			if (it != childrenComp.children.end()) {
				childrenComp.children.erase(it);
			}
			if (childrenComp.children.empty()) {
				RemoveComponent<ChildrenComponent>(parent);
			}
		}
	}

	// Runtime systems own resources that outlive their ECS components. Release
	// them while the entity and component data are still valid instead of making
	// each frame poll for orphaned handles after destruction.
	if (characterControllerSystem) {
		characterControllerSystem->RemoveController(entity);
	}
	if (physicsSystem) {
		physicsSystem->RemoveBody(entity, *this);
	}

	EntityGUIDRegistry::GetInstance().Unregister(entity);
	entityManager->DestroyEntity(entity);
	componentManager->EntityDestroyed(entity);
	systemManager->EntityDestroyed(entity);
	//ENGINE_PRINT("[ECSManager] Destroyed entity " , entity , ". Total active entities: " , entityManager->GetActiveEntityCount() , "\n");
}

void ECSManager::ClearAllEntities(bool clearGUIDRegistry) {
	// Bulk clears bypass DestroyEntity(), so release external runtime handles
	// explicitly before their backing components disappear.
	if (characterControllerSystem) {
		characterControllerSystem->Shutdown();
	}
	if (physicsSystem && physicsSystem->IsJoltInitialized()) {
		physicsSystem->Shutdown(*this);
	}

	// CRITICAL: Clear the GUID registry first to prevent duplicate entities during undo/redo
	//EntityGUIDRegistry::GetInstance().Clear();
	if (clearGUIDRegistry) {
		EntityGUIDRegistry::GetInstance().Clear();
	}

	entityManager->DestroyAllEntities();
	componentManager->AllEntitiesDestroyed();
	systemManager->AllEntitiesDestroyed();
	ENGINE_PRINT("[ECSManager] Cleared all entities and GUID registry. Total active entities: " , entityManager->GetActiveEntityCount(), "\n");
}

std::vector<Entity> ECSManager::GetAllRootEntities() {
	std::vector<Entity> rootEntities;
	for (const auto& entity : GetAllEntitiesView()) {
		if (!HasComponent<ParentComponent>(entity)) {
			rootEntities.push_back(entity);
		} 
	}

	return rootEntities;
}

void ECSManager::PreWarmActiveHierarchyCache() {
	PreWarmActiveHierarchyCache(GetActiveEntitiesView());
}

void ECSManager::PreWarmActiveHierarchyCache(const std::vector<Entity>& entitiesToWarm) {
	const std::uint64_t epoch = m_activeHierarchyEpoch.load(std::memory_order_relaxed);
	thread_local std::vector<Entity> hierarchyPath;
	hierarchyPath.clear();
	if (hierarchyPath.capacity() < 16) {
		hierarchyPath.reserve(16);
	}
	auto& guidRegistry = EntityGUIDRegistry::GetInstance();

	for (Entity entity : entitiesToWarm) {
		const std::uint64_t cachedValue = m_activeHierarchyCache[entity].load(std::memory_order_relaxed);
		if (IsActiveHierarchyCacheHit(cachedValue, epoch)) {
			continue;
		}

		hierarchyPath.clear();
		Entity currentEntity = entity;
		bool hierarchyActive = true;

		while (true) {
			if (currentEntity >= MAX_ENTITIES) {
				hierarchyActive = false;
				break;
			}

			const std::uint64_t currentCachedValue =
				m_activeHierarchyCache[currentEntity].load(std::memory_order_relaxed);
			if (IsActiveHierarchyCacheHit(currentCachedValue, epoch)) {
				hierarchyActive = CachedActiveHierarchyState(currentCachedValue);
				break;
			}

			// Parent cycles are invalid, but treating them as inactive keeps a bad
			// hierarchy from hanging the frame while it is being repaired.
			if (std::find(hierarchyPath.begin(), hierarchyPath.end(), currentEntity) != hierarchyPath.end()) {
				hierarchyActive = false;
				break;
			}
			hierarchyPath.push_back(currentEntity);

			const auto activeComponent = TryGetComponent<ActiveComponent>(currentEntity);
			if (activeComponent && !activeComponent->get().isActive) {
				hierarchyActive = false;
				break;
			}

			const auto parentComponent = TryGetComponent<ParentComponent>(currentEntity);
			if (!parentComponent) {
				break;
			}

			const GUID_128 parentGuid = parentComponent->get().parent;
			const Entity parentEntity = guidRegistry.GetEntityByGUID(parentGuid);
			if (parentEntity == static_cast<Entity>(-1) || parentEntity == UINT32_MAX) {
				break;
			}

			currentEntity = parentEntity;
		}

		for (Entity pathEntity : hierarchyPath) {
			m_activeHierarchyCache[pathEntity].store(
				PackActiveHierarchyState(epoch, hierarchyActive),
				std::memory_order_relaxed);
		}
	}
}

bool ECSManager::IsEntityActiveInHierarchy(Entity entity) {
	if (entity >= MAX_ENTITIES) {
		return false;
	}

	const std::uint64_t epoch = m_activeHierarchyEpoch.load(std::memory_order_relaxed);
	const std::uint64_t cachedValue = m_activeHierarchyCache[entity].load(std::memory_order_relaxed);
	if (IsActiveHierarchyCacheHit(cachedValue, epoch)) {
		return CachedActiveHierarchyState(cachedValue);
	}

	// Cache misses are uncommon after pre-warming. Keep one path allocation per
	// worker thread so an unexpected miss can also populate every ancestor it
	// traverses without allocating in subsequent frames.
	thread_local std::vector<Entity> hierarchyPath;
	hierarchyPath.clear();
	if (hierarchyPath.capacity() < 16) {
		hierarchyPath.reserve(16);
	}

	Entity currentEntity = entity;
	bool hierarchyActive = true;
	auto& guidRegistry = EntityGUIDRegistry::GetInstance();

	while (true) {
		if (currentEntity >= MAX_ENTITIES) {
			hierarchyActive = false;
			break;
		}

		const std::uint64_t currentCachedValue =
			m_activeHierarchyCache[currentEntity].load(std::memory_order_relaxed);
		if (IsActiveHierarchyCacheHit(currentCachedValue, epoch)) {
			hierarchyActive = CachedActiveHierarchyState(currentCachedValue);
			break;
		}

		if (std::find(hierarchyPath.begin(), hierarchyPath.end(), currentEntity) != hierarchyPath.end()) {
			hierarchyActive = false;
			break;
		}
		hierarchyPath.push_back(currentEntity);

		const auto activeComponent = TryGetComponent<ActiveComponent>(currentEntity);
		if (activeComponent && !activeComponent->get().isActive) {
			hierarchyActive = false;
			break;
		}

		const auto parentComponent = TryGetComponent<ParentComponent>(currentEntity);
		if (!parentComponent) {
			break;
		}

		const GUID_128 parentGuid = parentComponent->get().parent;
		const Entity parentEntity = guidRegistry.GetEntityByGUID(parentGuid);
		if (parentEntity == INVALID_ENTITY || parentEntity == UINT32_MAX) {
			break;
		}

		currentEntity = parentEntity;
	}

	const std::uint64_t packedValue = PackActiveHierarchyState(epoch, hierarchyActive);
	for (Entity pathEntity : hierarchyPath) {
		m_activeHierarchyCache[pathEntity].store(packedValue, std::memory_order_relaxed);
	}
	return hierarchyActive;
}
