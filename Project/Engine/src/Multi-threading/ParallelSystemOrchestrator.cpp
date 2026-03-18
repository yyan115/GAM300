#include "Multi-threading/ParallelSystemOrchestrator.hpp"
#include <TimeManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include "Physics/PhysicsSystem.hpp"
#include <Physics/Kinematics/CharacterControllerSystem.hpp>
#include "Logging.hpp"

void ParallelSystemOrchestrator::Update() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"UpdateChannel">, scheduler };
    auto& mainECS = ECSRegistry::GetInstance().GetActiveECSManager();

    // -------------------------------------------------------------------------
    // 1. LOGIC PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Scripts must run first to handle input and state changes.
    // Running this in parallel is risky due to potential logic race conditions.
    PROFILE_PLOT_TIMED("Script", mainECS.scriptSystem->Update());

    // Clear/Pre-warm cache for the parallel threads to read safely
    {
        PROFILE_SCOPED("HierarchyCache::PreWarm");
        mainECS.ClearActiveHierarchyCache();
        mainECS.PreWarmActiveHierarchyCache();
    }

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
            PROFILE_PLOT_TIMED("Animation",       mainECS.animationSystem->Update());
            PROFILE_PLOT_TIMED("SpriteAnimation", mainECS.spriteAnimationSystem->Update());
            });
    }

    // JOB B: Physics & Movement
    // Physics touches Root/Collider Entities. These are usually different
    // from Bones, so it is safe to run in parallel with Animation.
    frameChannel.Submit([&] {
        if (!gamePaused) {
            float dt = (float)TimeManager::GetDeltaTime();
            PROFILE_PLOT_TIMED("Physics",             mainECS.physicsSystem->Update(dt, mainECS));
            PROFILE_PLOT_TIMED("CharacterController", mainECS.characterControllerSystem->Update(dt, mainECS));
        }

        // Audio is usually thread-safe and light, fit it in the gap here
        if (mainECS.audioSystem) {
            PROFILE_PLOT_TIMED("Audio", mainECS.audioSystem->Update((float)TimeManager::GetDeltaTime()));
        }
        });

    // Wait for Simulation to finish before updating Transforms
    {
        PROFILE_SCOPED("SimulationJoin");
        frameChannel.join();
    }

    // -------------------------------------------------------------------------
    // 3. TRANSFORM PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Must run AFTER Physics/Anim so that World Matrices reflect this frame's changes.
    // This is the bottleneck (3.8ms), but it relies on the data from above.
    PROFILE_PLOT_TIMED("Transform", mainECS.transformSystem->Update());

    // Update anchors after transform so UI is positioned correctly
    PROFILE_PLOT_TIMED("UIAnchor", mainECS.uiAnchorSystem->Update());

    // OpenGL calls must be on main thread
    PROFILE_PLOT_TIMED("Video", mainECS.videoSystem->Update((float)TimeManager::GetDeltaTime()));

    // Refresh cache again if transforms changed hierarchy active states (rare but possible)
    {
        PROFILE_SCOPED("HierarchyCache::Refresh");
        mainECS.ClearActiveHierarchyCache();
        mainECS.PreWarmActiveHierarchyCache();
    }

    // -------------------------------------------------------------------------
    // 4. RENDER PREP PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Run these on the main thread to avoid complexity and overhead
    PROFILE_PLOT_TIMED("Camera",   mainECS.cameraSystem->Update());
    PROFILE_PLOT_TIMED("Lighting", mainECS.lightingSystem->Update());
    PROFILE_PLOT_TIMED("Button",   mainECS.buttonSystem->Update());
    PROFILE_PLOT_TIMED("Slider",   mainECS.sliderSystem->Update());
    PROFILE_PLOT_TIMED("Dialogue", mainECS.dialogueSystem->Update((float)TimeManager::GetDeltaTime()));
}

void ParallelSystemOrchestrator::Draw() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"DrawChannel">, scheduler };
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // Ensure cache is fully populated before parallel draw tasks (read-only is thread-safe)
    {
        PROFILE_SCOPED("HierarchyCache::DrawPreWarm");
        ecs.PreWarmActiveHierarchyCache();
    }

    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("Model", ecs.modelSystem->Update());
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("Text", ecs.textSystem->Update());
        });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("Sprite", ecs.spriteSystem->Update());
        });
    //frameChannel.Submit([&] {
    //    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    //    PROFILE_PLOT_TIMED("Particle", ecs.particleSystem->Update());
    //    });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("DebugDraw", ecs.debugDrawSystem->Update());
        });

    {
        PROFILE_SCOPED("DrawJoin");
        frameChannel.join(); // waits for actual work to finish
    }

    // Fog runs on the main thread (lazy init may create OpenGL VAO/VBO/EBO)
    if (ecs.fogSystem)
        PROFILE_PLOT_TIMED("Fog", ecs.fogSystem->Update());

    // Particle system runs on the main thread (lazy init may create OpenGL VAO/VBO/EBO)
    PROFILE_PLOT_TIMED("Particle", ecs.particleSystem->Update());

    // Set all isDirty flags to false after rendering
    PROFILE_PLOT_TIMED("PostUpdate", ecs.transformSystem->PostUpdate());
}