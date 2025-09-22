#include "EditorState.hpp"
#include <iostream>

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
    } else if (currentState == State::PAUSED) {
        SetState(State::PLAY_MODE);
    }
}

void EditorState::Pause() {
    if (GetState() == State::PLAY_MODE) {
        SetState(State::PAUSED);
    }
}

void EditorState::Stop() {
    SetState(State::EDIT_MODE);
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