#include "EditorState.hpp"
#include <iostream>
#include "Logging.hpp"
#include "TimeManager.hpp"

// Added includes for ECS and audio control
#include <ECS/ECSRegistry.hpp>
#include "Sound/AudioComponent.hpp"
#include "Sound/AudioManager.hpp"
#include "Scene/SceneManager.hpp"
#include "Physics/PhysicsSystem.hpp"
#include "Animation/AnimationComponent.hpp"
#include "Video/VideoComponent.hpp"


EditorState& EditorState::GetInstance() {
    static EditorState instance;
    return instance;
}

void EditorState::SetState(State newState) {
    State oldState = GetState();
    if (oldState != newState) {
        // Convert EditorState::State to Engine::GameState and delegate to Engine
        GameState engineState{};
        switch (newState) {
            case State::EDIT_MODE: engineState = GameState::EDIT_MODE; break;
            case State::PLAY_MODE: engineState = GameState::PLAY_MODE; break;
            case State::PAUSED: engineState = GameState::PAUSED_MODE; break;
        }
        Engine::SetGameState(engineState);

        // Log state changes for debugging
        const char* stateNames[] = { "EDIT_MODE", "PLAY_MODE", "PAUSED" };
        ENGINE_PRINT("[EditorState] State changed from ", stateNames[static_cast<int>(oldState)], " to ", stateNames[static_cast<int>(newState)], "\n");
    }
}

EditorState::State EditorState::GetState() const {
    // Convert Engine::GameState to EditorState::State
    GameState engineState = Engine::GetGameState();
    switch (engineState) {
        case GameState::EDIT_MODE: return State::EDIT_MODE;
        case GameState::PLAY_MODE: return State::PLAY_MODE;
        case GameState::PAUSED_MODE: return State::PAUSED;
        default: return State::EDIT_MODE;
    }
}

void EditorState::Play() {
    State currentState = GetState();
    if (currentState == State::EDIT_MODE) {
        // Save the current scene state before entering play mode
        SceneManager::GetInstance().SaveTempScene();

        SetState(State::PLAY_MODE);

        //SET PAUSE = FALSE
        TimeManager::SetPaused(false);


        //// Reset all animations to start fresh (ignore inspector preview state)
        //for (auto ent : ecs.GetActiveEntities()) {
        //    if (ecs.HasComponent<AnimationComponent>(ent)) {
        //        AnimationComponent& animComp = ecs.GetComponent<AnimationComponent>(ent);
        //        animComp.ResetForPlay(ent); // Reset animator to time 0 for fresh start
        //    }
        //}

        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();

        // Ensure FMOD global paused flag cleared so audio can play
        AudioManager::GetInstance().SetGlobalPaused(false);

        // Iterate all entities in the active ECS manager and trigger PlayOnAwake
        for (auto ent : ecs.GetActiveEntities()) {
            if (ecs.HasComponent<AudioComponent>(ent)) {
                AudioComponent& ac = ecs.GetComponent<AudioComponent>(ent);
                // If PlayOnAwake is set, ask the component to start (UpdateComponent handles PlayOnAwake)
                if (ac.PlayOnAwake) {
                    ac.UpdateComponent();
                }
            }
        }

        //RESET FLAG FOR VIDEOCOMPONENT
        for (auto ent : ecs.GetActiveEntities()) {
            if (ecs.HasComponent<VideoComponent>(ent)) {
                VideoComponent& videoComp = ecs.GetComponent<VideoComponent>(ent);
                videoComp.asset_dirty = true;
            }
        }

        ecs.animationSystem->Initialise();
        SceneManager::GetInstance().InitializeScenePhysics();

    } else if (currentState == State::PAUSED) {
        SetState(State::PLAY_MODE);

        // Unpause FMOD and resume components that were paused
        AudioManager::GetInstance().SetGlobalPaused(false);
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        for (auto ent : ecs.GetActiveEntities()) {
            if (ecs.HasComponent<AudioComponent>(ent)) {
                AudioComponent& ac = ecs.GetComponent<AudioComponent>(ent);
                ac.UnPause();
            }
        }
    }
}

void EditorState::Pause() {
    if (GetState() == State::PLAY_MODE) {
        SetState(State::PAUSED);

        // Pause FMOD and pause all playing audio components
        AudioManager::GetInstance().SetGlobalPaused(true);
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        for (auto ent : ecs.GetActiveEntities()) {
            if (ecs.HasComponent<AudioComponent>(ent)) {
                AudioComponent& ac = ecs.GetComponent<AudioComponent>(ent);
                ac.Pause();
            }
        }
    }
}

void EditorState::Stop() {
    // Stop all audio playback in FMOD and reset components
    AudioManager::GetInstance().StopAll();
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    for (auto ent : ecs.GetActiveEntities()) {
        if (ecs.HasComponent<AudioComponent>(ent)) {
            AudioComponent& ac = ecs.GetComponent<AudioComponent>(ent);
            ac.Stop();
        }
    }

    SceneManager::GetInstance().ShutDownScenePhysics();

    // Reload the scene to the saved state before play mode
    SceneManager::GetInstance().ReloadTempScene();

    // Re-acquire ECS reference after scene reload (old reference is now stale)
    ECSManager& ecsAfterReload = ECSRegistry::GetInstance().GetActiveECSManager();

    // Reset all animation preview states to 0 (fresh editor state)
    for (auto ent : ecsAfterReload.GetActiveEntities()) {
        if (ecsAfterReload.HasComponent<AnimationComponent>(ent)) {
            AnimationComponent& animComp = ecsAfterReload.GetComponent<AnimationComponent>(ent);
            animComp.ResetPreview(ent);
        }
    }

    SetState(State::EDIT_MODE);
}

void EditorState::SetSelectedEntity(Entity entity) {
    if (selectedEntity != entity) {
        selectedEntity = entity;
        ENGINE_PRINT("[EditorState] Selected entity: ", entity , "\n");
    }
}

void EditorState::ClearSelection() {
    if (selectedEntity != INVALID_ENTITY) {
        ENGINE_PRINT("[EditorState] Cleared selection\n");
        selectedEntity = INVALID_ENTITY;
    }
}