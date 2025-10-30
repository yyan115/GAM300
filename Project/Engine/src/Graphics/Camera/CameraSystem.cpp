/* Start Header ************************************************************************/
/*!
\file       CameraSystem.cpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu
\date       
\brief      

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#include "pch.h"
#include "Graphics/Camera/CameraSystem.hpp"
#include "Graphics/Camera/CameraComponent.hpp"
#include "Transform/TransformComponent.hpp"
#include "ECS/ECSRegistry.hpp"
#include "ECS/ActiveComponent.hpp"
#include "Logging.hpp"
#include "TimeManager.hpp"

bool CameraSystem::Initialise()
{
	activeCamera = std::make_unique<Camera>();

	activeCameraEntity = FindHighestPriorityCamera();
	if (activeCameraEntity != 0)
	{
		UpdateCameraFromComponent(activeCameraEntity);
		ENGINE_PRINT("[CameraSystem] Initialized with active camera entity: ", activeCameraEntity, "\n");
	}
	else
	{
		ENGINE_PRINT("[CameraSystem] No active camera found, using default\n");
	}

	return true;
}

void CameraSystem::Update()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	// Validate that active camera entity still exists
	if (activeCameraEntity != 0 && entities.find(activeCameraEntity) == entities.end())
	{
		// Active camera was deleted, find a new one
		ENGINE_PRINT("[CameraSystem] Active camera entity ", activeCameraEntity, " no longer exists, finding new camera\n");
		activeCameraEntity = 0;
	}

	// Check if current active camera entity has become inactive (Unity-like behavior)
	if (activeCameraEntity != 0 && ecsManager.HasComponent<ActiveComponent>(activeCameraEntity))
	{
		auto& activeComp = ecsManager.GetComponent<ActiveComponent>(activeCameraEntity);
		if (!activeComp.isActive)
		{
			// Active camera entity was disabled, need to find a new one
			ENGINE_PRINT("[CameraSystem] Active camera entity ", activeCameraEntity, " was disabled, finding new camera\n");
			activeCameraEntity = 0; // Force re-evaluation
		}
	}

	Entity highestPriority = FindHighestPriorityCamera();

	// Check if active camera changed (higher priority became active or current was deleted/disabled)
	if (highestPriority != activeCameraEntity)
	{
		Entity oldCamera = activeCameraEntity;
		activeCameraEntity = highestPriority;

		if (activeCameraEntity != 0)
		{
			ENGINE_PRINT("[CameraSystem] Switched camera from entity ", oldCamera, " to entity ", activeCameraEntity, "\n");
			// Immediately update the camera to ensure view switches
			UpdateCameraFromComponent(activeCameraEntity);
		}
		else
		{
			// No active camera available
			ENGINE_PRINT("[CameraSystem] No active camera found (all cameras disabled or deleted)\n");
		}
	}

	// Update all active cameras (for shake effects)
	float deltaTime = (float)TimeManager::GetDeltaTime();
	for (const auto& entity : entities)
	{
		if (ecsManager.HasComponent<CameraComponent>(entity))
		{
			auto& camComp = ecsManager.GetComponent<CameraComponent>(entity);
			if (camComp.isActive)
			{
				UpdateCameraShake(entity, deltaTime);
			}
		}
	}

	// Update active camera if we have one
	if (activeCameraEntity != 0 && entities.find(activeCameraEntity) != entities.end())
	{
		UpdateCameraFromComponent(activeCameraEntity);
	}
}

void CameraSystem::Shutdown()
{
	activeCamera.reset();
	ENGINE_PRINT("[CameraSystem] Shutdown\n");
}

void CameraSystem::SetActiveCamera(Entity entity)
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	if (entities.find(entity) == entities.end()) 
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[CameraSystem] Entity ", entity, " is not a camera!\n");
		return;
	}

	// Deactivate all cameras
	for (const auto& e : entities)
	{
		// Safety check: ensure component still exists
		if (!ecsManager.HasComponent<CameraComponent>(e))
			continue;

		auto& camComp = ecsManager.GetComponent<CameraComponent>(e);
		camComp.isActive = false;
	}

	auto& camComp = ecsManager.GetComponent<CameraComponent>(entity);
	camComp.isActive = true;
	activeCameraEntity = entity;

	UpdateCameraFromComponent(entity);
}

void CameraSystem::UpdateCameraFromComponent(Entity entity)
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	// Safety check: ensure components still exist (entity might be in process of deletion)
	if (!ecsManager.HasComponent<CameraComponent>(entity) || !ecsManager.HasComponent<Transform>(entity))
		return;

	auto& camComp = ecsManager.GetComponent<CameraComponent>(entity);
	auto& transform = ecsManager.GetComponent<Transform>(entity);

	glm::vec3 worldPos = glm::vec3(transform.worldMatrix.m.m03, transform.worldMatrix.m.m13, transform.worldMatrix.m.m23);

	// Update Camera object
	activeCamera->Position = worldPos; 
	activeCamera->WorldUp = camComp.up; 
	activeCamera->MovementSpeed = camComp.movementSpeed; 
	activeCamera->MouseSensitivity = camComp.mouseSensitivity; 
	activeCamera->Zoom = camComp.fov;

	if (camComp.useFreeRotation) 
	{
		activeCamera->Yaw = camComp.yaw;
		activeCamera->Pitch = camComp.pitch;
		// Calculate front vector here instead
		glm::vec3 front;
		front.x = cos(glm::radians(camComp.yaw)) * cos(glm::radians(camComp.pitch));
		front.y = sin(glm::radians(camComp.pitch));
		front.z = sin(glm::radians(camComp.yaw)) * cos(glm::radians(camComp.pitch));
		activeCamera->Front = glm::normalize(front);
	}
	else 
	{
		activeCamera->Front = glm::normalize(camComp.target);
	}

	activeCamera->Right = glm::normalize(glm::cross(activeCamera->Front, activeCamera->WorldUp)); 
	activeCamera->Up = glm::normalize(glm::cross(activeCamera->Right, activeCamera->Front)); 
}

Entity CameraSystem::FindHighestPriorityCamera()
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();

	Entity highest = 0;
	int highestPriority = -999999;

	// First pass: find active camera with highest priority (skip inactive entities)
	for (const auto& entity : entities)
	{
		// Safety check: ensure component still exists (entity might be in process of deletion)
		if (!ecsManager.HasComponent<CameraComponent>(entity))
			continue;

		// Skip cameras on inactive entities (Unity-like behavior)
		if (ecsManager.HasComponent<ActiveComponent>(entity)) {
			auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
			if (!activeComp.isActive) {
				continue; // Skip inactive entities
			}
		}

		auto& camComp = ecsManager.GetComponent<CameraComponent>(entity);

		if (camComp.isActive && camComp.priority > highestPriority)
		{
			highest = entity;
			highestPriority = camComp.priority;
		}
	}

	// If no active camera found, activate the highest priority camera that exists (and is on an active entity)
	if (highest == 0 && !entities.empty())
	{
		highestPriority = -999999;
		for (const auto& entity : entities)
		{
			if (!ecsManager.HasComponent<CameraComponent>(entity))
				continue;

			// Skip cameras on inactive entities
			if (ecsManager.HasComponent<ActiveComponent>(entity)) {
				auto& activeComp = ecsManager.GetComponent<ActiveComponent>(entity);
				if (!activeComp.isActive) {
					continue;
				}
			}

			auto& camComp = ecsManager.GetComponent<CameraComponent>(entity);

			if (camComp.priority > highestPriority)
			{
				highest = entity;
				highestPriority = camComp.priority;
			}
		}

		// Activate this camera
		if (highest != 0)
		{
			auto& camComp = ecsManager.GetComponent<CameraComponent>(highest);
			camComp.isActive = true;
			ENGINE_PRINT("[CameraSystem] No active camera found, auto-activated camera entity: ", highest, "\n");
		}
	}

	return highest;
}

void CameraSystem::UpdateCameraShake(Entity entity, float deltaTime)
{
	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& camComp = ecsManager.GetComponent<CameraComponent>(entity);

	if (!camComp.isShaking)
	{
		camComp.shakeOffset = glm::vec3(0.f);
		return;
	}

	camComp.shakeTimer += deltaTime;
	
	if (camComp.shakeTimer >= camComp.shakeDuration)
	{
		camComp.isShaking = false;
		camComp.shakeOffset = glm::vec3(0.f);
		camComp.shakeTimer = 0.f;
		return;
	}

	float decay = 1.f - (camComp.shakeTimer / camComp.shakeDuration);
	float currentIntensity = camComp.shakeIntensity * decay;

	float time = camComp.shakeTimer * camComp.shakeFrequency;
	camComp.shakeOffset.x = sin(time * 1.3f) * currentIntensity;
	camComp.shakeOffset.y = sin(time * 1.7f) * currentIntensity;
	camComp.shakeOffset.z = sin(time * 1.1f) * currentIntensity * 0.5f; // Less Z shake
}

void CameraSystem::ZoomCamera(Entity cameraEntity, float zoomDelta)
{
	if (entities.find(cameraEntity) == entities.end())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[CameraSystem] Entity ", cameraEntity, " is not a camera!\n");
		return;
	}

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& camComp = ecsManager.GetComponent<CameraComponent>(cameraEntity);

	if (camComp.projectionType == ProjectionType::PERSPECTIVE)
	{
		// Zoom by changing FOV
		camComp.fov -= zoomDelta * camComp.zoomSpeed;
		camComp.fov = glm::clamp(camComp.fov, camComp.minZoom, camComp.maxZoom);
	}
	else
	{
		// Orthographic zoom by changing ortho size
		camComp.orthoSize -= zoomDelta * camComp.zoomSpeed * 0.1f;
		camComp.orthoSize = glm::clamp(camComp.orthoSize, camComp.minZoom, camComp.maxZoom);
	}
}

void CameraSystem::SetZoom(Entity cameraEntity, float zoomLevel)
{
	if (entities.find(cameraEntity) == entities.end())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[CameraSystem] Entity ", cameraEntity, " is not a camera!\n");
		return;
	}

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& camComp = ecsManager.GetComponent<CameraComponent>(cameraEntity);

	if (camComp.projectionType == ProjectionType::PERSPECTIVE)
	{
		camComp.fov = glm::clamp(zoomLevel, camComp.minZoom, camComp.maxZoom);
	}
	else
	{
		camComp.orthoSize = glm::clamp(zoomLevel, camComp.minZoom, camComp.maxZoom);
	}
}

void CameraSystem::ShakeCamera(Entity cameraEntity, float intensity, float duration)
{
	if (entities.find(cameraEntity) == entities.end())
	{
		ENGINE_PRINT(EngineLogging::LogLevel::Error, "[CameraSystem] Entity ", cameraEntity, " is not a camera!\n");
		return;
	}

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& camComp = ecsManager.GetComponent<CameraComponent>(cameraEntity);

	camComp.isShaking = true;
	camComp.shakeIntensity = intensity;
	camComp.shakeDuration = duration;
	camComp.shakeTimer = 0.f;
}

void CameraSystem::StopShake(Entity cameraEntity)
{
	if (entities.find(cameraEntity) == entities.end()) return;

	ECSManager& ecsManager = ECSRegistry::GetInstance().GetActiveECSManager();
	auto& camComp = ecsManager.GetComponent<CameraComponent>(cameraEntity);

	camComp.isShaking = false;
	camComp.shakeOffset = glm::vec3(0.f);
	camComp.shakeTimer = 0.f;
}