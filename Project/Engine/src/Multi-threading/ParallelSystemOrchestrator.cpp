#include "Multi-threading/ParallelSystemOrchestrator.hpp"
#include <TimeManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include "Physics/PhysicsSystem.hpp"
#include <Physics/Kinematics/CharacterControllerSystem.hpp>

void ParallelSystemOrchestrator::Update() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"UpdateChannel">, scheduler };
    auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

    // -------------------------------------------------------------------------
    // 1. LOGIC PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Scripts must run first to handle input and state changes.
    // Running this in parallel is risky due to potential logic race conditions.
    mainECS.scriptSystem->Update();

    // Clear/Pre-warm cache for the parallel threads to read safely
    mainECS.ClearActiveHierarchyCache();
    mainECS.PreWarmActiveHierarchyCache();

    bool gamePaused = TimeManager::IsPaused();

    // -------------------------------------------------------------------------
    // 2. SIMULATION PHASE (Parallel)
    // -------------------------------------------------------------------------
    // We group the heavy systems to run simultaneously.
    // Thread 1: Animation (3.1ms)
    // Thread 2: Physics + CC (2.25ms) + Audio (Light)
    // -------------------------------------------------------------------------

    // JOB A: Animation
    if (!gamePaused) {
        frameChannel.Submit([&] {
            // Animation touches Bone Entities
            mainECS.animationSystem->Update();
            mainECS.spriteAnimationSystem->Update();
            });
    }

    // JOB B: Physics & Movement
    // Physics touches Root/Collider Entities. These are usually different 
    // from Bones, so it is safe to run in parallel with Animation.
    frameChannel.Submit([&] {
        if (!gamePaused) {
            mainECS.physicsSystem->Update((float)TimeManager::GetDeltaTime(), mainECS);
            mainECS.characterControllerSystem->Update((float)TimeManager::GetDeltaTime(), mainECS);
        }

        // Audio is usually thread-safe and light, fit it in the gap here
        if (mainECS.audioSystem) {
            mainECS.audioSystem->Update((float)TimeManager::GetDeltaTime());
        }
        });

    // Wait for Simulation to finish before updating Transforms
    frameChannel.join();
    //std::cout << "[ParallelSystemOrchestrator] JOB A & B: Animation, Physics & Movement complete" << std::endl;

    // -------------------------------------------------------------------------
    // 3. TRANSFORM PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Must run AFTER Physics/Anim so that World Matrices reflect this frame's changes.
    // This is the bottleneck (3.8ms), but it relies on the data from above.
    mainECS.transformSystem->Update();
	//std::cout << "[ParallelSystemOrchestrator] TRANSFORM PHASE complete" << std::endl;

    // Update anchors after transform so UI is positioned correctly
    mainECS.uiAnchorSystem->Update();
	//std::cout << "[ParallelSystemOrchestrator] UI ANCHOR UPDATE complete" << std::endl;

    // OpenGL calls must be on main thread
    mainECS.videoSystem->Update((float)TimeManager::GetDeltaTime());
	//std::cout << "[ParallelSystemOrchestrator] VIDEO SYSTEM UPDATE complete" << std::endl;

    // Refresh cache again if transforms changed hierarchy active states (rare but possible)
    mainECS.ClearActiveHierarchyCache();
    mainECS.PreWarmActiveHierarchyCache();

    // -------------------------------------------------------------------------
    // 4. RENDER PREP PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Run these on the main thread to avoid complexity and overhead
    mainECS.cameraSystem->Update();
    mainECS.lightingSystem->Update();

    mainECS.buttonSystem->Update();
    mainECS.sliderSystem->Update();

    //std::cout << "[ParallelSystemOrchestrator] RENDER PREP PHASE complete" << std::endl;
}

void ParallelSystemOrchestrator::Draw() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"DrawChannel">, scheduler };
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // Ensure cache is fully populated before parallel draw tasks (read-only is thread-safe)
    ecs.PreWarmActiveHierarchyCache();

    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running ModelJob");
        ecs.modelSystem->Update();
        //ENGINE_PRINT("ModelJob finished");
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running TextJob");
        ecs.textSystem->Update();
       // ENGINE_PRINT("TextJob finished");
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running SpriteJob");
        ecs.spriteSystem->Update();
        //ENGINE_PRINT("SpriteJob finished");
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running ParticleJob");
        ecs.particleSystem->Update();
        //ENGINE_PRINT("ParticleJob finished");
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        //ENGINE_LOG_DEBUG("Running DebugDrawJob");
        ecs.debugDrawSystem->Update();
        //ENGINE_PRINT("DebugDrawJob finished");
        });

    frameChannel.join(); // waits for actual work to finish
    //ENGINE_LOG_DEBUG("Draw Synchronized\n");

    // Set all isDirty flags to false after rendering
    ecs.transformSystem->PostUpdate();
}