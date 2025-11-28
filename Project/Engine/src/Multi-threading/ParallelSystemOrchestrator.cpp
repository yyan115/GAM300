#include "Multi-threading/ParallelSystemOrchestrator.hpp"
#include <TimeManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include "Physics/PhysicsSystem.hpp"

void ParallelSystemOrchestrator::Update() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"UpdateChannel">, scheduler };

    // Update physics and transform systems sequentially first.
    auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();
    mainECS.physicsSystem->Update((float)TimeManager::GetFixedDeltaTime(), mainECS);
    mainECS.transformSystem->Update();

	// Then update the other systems in parallel.
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running AudioJob");
        ecs.audioSystem->Update((float)TimeManager::GetDeltaTime());
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running AnimationJob");
        ecs.animationSystem->Update();
		});
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running CameraJob");
        ecs.cameraSystem->Update();
		});
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running LightingJob");
        ecs.lightingSystem->Update();
        });
	frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running ScriptingJob");
        ecs.scriptSystem->Update();
		});
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running ButtonJob");
        ecs.buttonSystem->Update();
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running SpriteAnimationJob");
        ecs.spriteAnimationSystem->Update();
		});

    // Synchronize
    frameChannel.join();
    //ENGINE_LOG_DEBUG("Update Synchronized\n");
}

void ParallelSystemOrchestrator::Draw() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"DrawChannel">, scheduler };

    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running ModelJob");
        ecs.modelSystem->Update();
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running TextJob");
        ecs.textSystem->Update();
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running SpriteJob");
        ecs.spriteSystem->Update();
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running ParticleJob");
        ecs.particleSystem->Update();
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running DebugDrawJob");
        ecs.debugDrawSystem->Update();
        });

    frameChannel.join(); // waits for actual work to finish
    //ENGINE_LOG_DEBUG("Draw Synchronized\n");
}