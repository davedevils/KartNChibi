// IGameState.h - base interface for all game states
#pragma once

#include "Types.h"

class IGameState {
public:
    virtual ~IGameState() = default;
    
    // lifecycle
    virtual bool Initialize() = 0;
    virtual void Cleanup() = 0;
    
    // state transitions
    virtual void OnEnter() = 0;
    virtual void OnExit() = 0;
    
    // frame update
    virtual void Update(float deltaTime) = 0;
    virtual void Render() = 0;
    
    // input handling (optional override)
    virtual void OnKeyDown(int key) {}
    virtual void OnKeyUp(int key) {}
    virtual void OnMouseMove(int x, int y) {}
    virtual void OnMouseDown(int button) {}
    virtual void OnMouseUp(int button) {}
};

