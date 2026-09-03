#include "Multi-threading/ParallelSystemOrchestrator.hpp"
#include <TimeManager.hpp>
#include <ECS/ECSRegistry.hpp>
#include "Physics/PhysicsSystem.hpp"
#include <Physics/Kinematics/CharacterControllerSystem.hpp>
#include "Graphics/GraphicsManager.hpp"
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

    // Scripts may have changed ActiveComponent values. Warm only the entities
    // consumed by parallel workers instead of traversing the entire scene.
    {
        PROFILE_SCOPED("HierarchyCache::Reset");
        { PROFILE_SCOPED("HC::Clear"); mainECS.ClearActiveHierarchyCache(); }
        {
            PROFILE_SCOPED("HC::WarmParallelSimulation");
            if (mainECS.animationSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.animationSystem->entities.DenseView());
            if (mainECS.spriteAnimationSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.spriteAnimationSystem->entities.DenseView());
            if (mainECS.physicsSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.physicsSystem->entities.DenseView());
            if (mainECS.characterControllerSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.characterControllerSystem->entities.DenseView());
            if (mainECS.audioSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.audioSystem->entities.DenseView());
        }
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

    // Wait for Simulation to finish before updating Transforms.
    {
        PROFILE_SCOPED("SimulationJoin");
        { PROFILE_SCOPED("SJ::WaitForJobs"); frameChannel.join(); }
    }

    // -------------------------------------------------------------------------
    // 3. TRANSFORM PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Anchors write local transforms, so apply them before propagating world
    // matrices. Transform must still run after Physics/Animation.
    PROFILE_PLOT_TIMED("UIAnchor", mainECS.uiAnchorSystem->Update());
    PROFILE_PLOT_TIMED("Transform", mainECS.transformSystem->Update());

    // OpenGL calls must be on main thread
    PROFILE_PLOT_TIMED("Video", mainECS.videoSystem->Update((float)TimeManager::GetDeltaTime()));

    // Refresh cache again if transforms changed hierarchy active states (rare but possible)
    //{
    //    PROFILE_SCOPED("HierarchyCache::Refresh");
    //    { PROFILE_SCOPED("HC::Clear"); mainECS.ClearActiveHierarchyCache(); }
    //    { PROFILE_SCOPED("HC::Warm");  mainECS.PreWarmActiveHierarchyCache(); }
    //}

    // -------------------------------------------------------------------------
    // 4. RENDER PREP PHASE (Sequential)
    // -------------------------------------------------------------------------
    // Run these on the main thread to avoid complexity and overhead
    PROFILE_PLOT_TIMED("Camera",   mainECS.cameraSystem->Update());
    if (auto* activeCamera = mainECS.cameraSystem->GetActiveCamera())
    {
        auto& graphics = GraphicsManager::GetInstance();
        graphics.SetCamera(activeCamera);
        graphics.UpdateFrustum();
    }
    PROFILE_PLOT_TIMED("Lighting", mainECS.lightingSystem->Update());
    PROFILE_PLOT_TIMED("Button",   mainECS.buttonSystem->Update());
    PROFILE_PLOT_TIMED("Slider",   mainECS.sliderSystem->Update());
    PROFILE_PLOT_TIMED("Dialogue", mainECS.dialogueSystem->Update((float)TimeManager::GetDeltaTime()));

    // UI callbacks and dialogue can toggle entities after the simulation phase.
    // Warm only the model/sprite sets that the parallel draw workers consume.
    {
        PROFILE_SCOPED("HierarchyCache::DrawPreWarm");
        { PROFILE_SCOPED("HC::DrawClear"); mainECS.ClearActiveHierarchyCache(); }
        {
            PROFILE_SCOPED("HC::DrawWarm");
            if (mainECS.modelSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.modelSystem->entities.DenseView());
            if (mainECS.spriteSystem)
                mainECS.PreWarmActiveHierarchyCache(mainECS.spriteSystem->entities.DenseView());
        }
    }
}

void ParallelSystemOrchestrator::Draw() {
    xscheduler::task_group frameChannel{ xscheduler::str_v<"DrawChannel">, scheduler };
    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

    // Ensure cache is fully populated before parallel draw tasks (read-only is thread-safe)
    //{
    //    PROFILE_SCOPED("HierarchyCache::DrawPreWarm");
    //    ecs.PreWarmActiveHierarchyCache();
    //}

    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("Model", ecs.modelSystem->Update());
        });
    //frameChannel.Submit([&] {
    //    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    //    PROFILE_PLOT_TIMED("Text", ecs.textSystem->Update());
    //    });
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("Sprite", ecs.spriteSystem->Update());
        });
    //frameChannel.Submit([&] {
    //    auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    //    PROFILE_PLOT_TIMED("Particle", ecs.particleSystem->Update());
    //    });
#ifndef ANDROID
    frameChannel.Submit([&] {
        auto& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        PROFILE_PLOT_TIMED("DebugDraw", ecs.debugDrawSystem->Update());
        });
#endif

    {
        PROFILE_SCOPED("DrawJoin");
        frameChannel.join(); // waits for actual work to finish
    }

	// Text system runs on the main thread (lazy init may create OpenGL VAO/VBO/EBO)
    PROFILE_PLOT_TIMED("Text", ecs.textSystem->Update());

    // Fog runs on the main thread (lazy init may create OpenGL VAO/VBO/EBO)
#ifndef ANDROID
    if (ecs.fogSystem)
        PROFILE_PLOT_TIMED("Fog", ecs.fogSystem->Update());
#endif

    // Particle system runs on the main thread (lazy init may create OpenGL VAO/VBO/EBO)
    PROFILE_PLOT_TIMED("Particle", ecs.particleSystem->Update());

}
