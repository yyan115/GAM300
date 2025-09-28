#include "EditorState.hpp"
#include <iostream>

// Added includes for ECS and audio control
#include <ECS/ECSRegistry.hpp>
#include <Sound/AudioComponent.hpp>
#include <Sound/AudioSystem.hpp>

EditorState& EditorState::GetInstance() {
    static EditorState instance;
    return instance;
}

void EditorState::SetState(State newState) {
    State oldState = GetState();
    if (oldState != newState) {
        // Convert EditorState::State to Engine::GameState and delegate to Engine
        GameState engineState;
        switch (newState) {
            case State::EDIT_MODE: engineState = GameState::EDIT_MODE; break;
            case State::PLAY_MODE: engineState = GameState::PLAY_MODE; break;
            case State::PAUSED: engineState = GameState::PAUSED_MODE; break;
        }
        Engine::SetGameState(engineState);

        // Log state changes for debugging
        const char* stateNames[] = { "EDIT_MODE", "PLAY_MODE", "PAUSED" };
        std::cout << "[EditorState] State changed from " << stateNames[static_cast<int>(oldState)]
                  << " to " << stateNames[static_cast<int>(newState)] << std::endl;
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
        SetState(State::PLAY_MODE);

        // Ensure FMOD global paused flag cleared so audio can play
        AudioSystem::GetInstance().SetGlobalPaused(false);

        // Iterate all entities in the active ECS manager and trigger PlayOnAwake
        ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
        for (auto ent : ecs.GetActiveEntities()) {
            if (ecs.HasComponent<AudioComponent>(ent)) {
                AudioComponent& ac = ecs.GetComponent<AudioComponent>(ent);
                // If PlayOnAwake is set, ask the component to start (UpdateComponent handles PlayOnAwake)
                if (ac.PlayOnStart) {
                    ac.UpdateComponent();
                }
            }
        }
    } else if (currentState == State::PAUSED) {
        SetState(State::PLAY_MODE);

        // Unpause FMOD and resume components that were paused
        AudioSystem::GetInstance().SetGlobalPaused(false);
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
        AudioSystem::GetInstance().SetGlobalPaused(true);
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
    SetState(State::EDIT_MODE);

    // Stop all audio playback in FMOD and reset components
    AudioSystem::GetInstance().StopAll();
    ECSManager& ecs = ECSRegistry::GetInstance().GetActiveECSManager();
    for (auto ent : ecs.GetActiveEntities()) {
        if (ecs.HasComponent<AudioComponent>(ent)) {
            AudioComponent& ac = ecs.GetComponent<AudioComponent>(ent);
            ac.Stop();
        }
    }
}

void EditorState::SetSelectedEntity(Entity entity) {
    if (selectedEntity != entity) {
        selectedEntity = entity;
        std::cout << "[EditorState] Selected entity: " << entity << std::endl;
    }
}

void EditorState::ClearSelection() {
    if (selectedEntity != INVALID_ENTITY) {
        std::cout << "[EditorState] Cleared selection" << std::endl;
        selectedEntity = INVALID_ENTITY;
    }
}