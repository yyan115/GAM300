/* Start Header ************************************************************************/
/*!
\file       CameraSystem.hpp
\author     TAN SHUN ZHI, Tomy, t.shunzhitomy, 2301341, t.shunzhitomy@digipen.edu
\date
\brief

Copyright (C) 2025 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
*/
/* End Header **************************************************************************/
#pragma once
#include "ECS/System.hpp"
#include "Camera.h"
#include <memory>

class CameraSystem : public System {
public:
	CameraSystem() = default;
	~CameraSystem() = default;

	bool Initialise();
	void Update();
	void Shutdown();

	Entity GetActiveCameraEntity() const { return activeCameraEntity; }

	void SetActiveCamera(Entity entity);

	Camera* GetActiveCamera() { return activeCamera.get(); }

private:
	Entity activeCameraEntity = 0;
	std::unique_ptr<Camera> activeCamera;

	void UpdateCameraFromComponent(Entity entity);
	Entity FindHighestPriorityCamera();
};