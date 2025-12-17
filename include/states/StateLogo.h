// StateLogo.h - logo/splash screen state
#pragma once

#include "states/IGameState.h"

struct IDirect3DTexture9;

class StateLogo : public IGameState {
public:
    StateLogo();
    ~StateLogo() override;
    
    bool Initialize() override;
    void Cleanup() override;
    
    void OnEnter() override;
    void OnExit() override;
    
    void Update(float deltaTime) override;
    void Render() override;
    
    void OnKeyDown(int key) override;
    
private:
    float m_timer = 0.0f;
    int m_phase = 0;  // 0=company logo, 1=game logo, 2=connecting
    bool m_connecting = false;
    
    IDirect3DTexture9* m_bgTexture = nullptr;
    IDirect3DTexture9* m_logoTexture = nullptr;
    IDirect3DTexture9* m_companyLogo = nullptr;
};


