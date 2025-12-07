/*********************************************************************************
* @File			RigidBodyComponent.hpp
* @Author		Ang Jia Jun Austin, a.jiajunaustin@digipen.edu
* @Co-Author	-
* @Date			25/10/2025
* @Brief		Rigid body component for physics simulation. Defines physical
*				properties and motion behavior for entities in the physics system.
*
* Copyright (C) 2025 DigiPen Institute of Technology. Reproduction or disclosure
* of this file or its contents without the prior written consent of DigiPen
* Institute of Technology is prohibited.
*********************************************************************************/

#pragma once
#include "Physics/JoltInclude.hpp"
#include "Reflection/ReflectionBase.hpp"
#include "Math/Vector3D.hpp"

enum class Motion : int { 
	Static = 0, 
	Kinematic, 
	Dynamic 
};

struct PhysicsMaterial {
	std::string name = "Default";
	float friction = 0.6f;
};



struct RigidBodyComponent {
	REFL_SERIALIZABLE
	bool enabled = true;          // Component enabled state (can be toggled in inspector)
	int motionID{};

	bool ccd		= false; // continuous collision detection
	bool isTrigger	= false;

	float gravityFactor = 1.0f;

	Vector3D angularVel = { 0.0f,0.0f,0.0f };
	Vector3D linearVel = { 0.0f, -9.81f,0.0f };

	float linearDamping = 0.0f;
	float angularDamping = 0.95f;


	//TO BE USED FOR SCRIPT
	Vector3D forceApplied = { 0.0f,0.0f,0.0f };
	Vector3D torqueApplied = { 0.0f,0.0f,0.0f };
	Vector3D impulseApplied = { 0.0f,0.0f,0.0f };
	void AddForce(Vector3D force)		{forceApplied += force;}
	void AddTorque(Vector3D torque)		{torqueApplied += torque;}
	void AddImpulse(Vector3D impulse)	{impulseApplied += impulse;}



	Motion motion{};
	bool transform_dirty = false;     // set by gameplay when you edit Transform of kinematic/static
	bool motion_dirty = false;        // if you change Motion, flip this to trigger recreate
	uint32_t collider_seen_version = 0; // last applied Collider::version
	JPH::BodyID id = JPH::BodyID();

	RigidBodyComponent() = default;
	~RigidBodyComponent() = default;

	void SetEnabled(bool e) { enabled = e; }
	bool IsEnabled() const { return enabled; }
};