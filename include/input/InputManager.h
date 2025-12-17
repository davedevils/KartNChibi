// InputManager.h - DirectInput keyboard/mouse handling
#pragma once

#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <cstdint>

class InputManager {
public:
    InputManager();
    ~InputManager();
    
    bool Initialize(HINSTANCE hInst, HWND hWnd);
    void Update();
    void Shutdown();
    
    // keyboard
    bool IsKeyDown(int key) const;
    bool IsKeyPressed(int key) const;   // just pressed this frame
    bool IsKeyReleased(int key) const;  // just released this frame
    
    // mouse
    int GetMouseX() const { return m_mouseX; }
    int GetMouseY() const { return m_mouseY; }
    int GetMouseDeltaX() const { return m_mouseDX; }
    int GetMouseDeltaY() const { return m_mouseDY; }
    bool IsMouseButtonDown(int button) const;
    
private:
    bool InitKeyboard(HINSTANCE hInst, HWND hWnd);
    bool InitMouse(HINSTANCE hInst, HWND hWnd);
    
    IDirectInput8* m_pDI = nullptr;
    IDirectInputDevice8* m_pKeyboard = nullptr;
    IDirectInputDevice8* m_pMouse = nullptr;
    
    uint8_t m_keyState[256] = {};
    uint8_t m_prevKeyState[256] = {};
    
    DIMOUSESTATE m_mouseState = {};
    int m_mouseX = 0;
    int m_mouseY = 0;
    int m_mouseDX = 0;
    int m_mouseDY = 0;
    
    HWND m_hWnd = nullptr;
    bool m_initialized = false;
};


