// StateManager.h - game state machine
#pragma once

#include "Types.h"
#include <memory>

class IGameState;

class StateManager {
public:
    StateManager();
    ~StateManager();
    
    void SetState(GameState newState);
    GameState GetState() const { return m_currentState; }
    
    void Update(float deltaTime);
    void Render();
    
    IGameState* GetCurrentHandler() { return m_handler.get(); }
    
private:
    void CleanupState();
    void InitializeState();
    
private:
    GameState m_currentState = GameState::None;
    std::unique_ptr<IGameState> m_handler;
};

