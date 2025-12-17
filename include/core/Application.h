// Application.h - main application class
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <memory>

class StateManager;
class EngineWrapper;
class Connection;
class PacketDispatcher;
class InputManager;

class Application {
public:
    Application();
    ~Application();
    
    bool Initialize(HINSTANCE hInstance);
    int Run();
    void Shutdown();
    
    HWND GetWindow() const { return m_hWnd; }
    HINSTANCE GetInstance() const { return m_hInstance; }
    
    StateManager* GetStateManager() { return m_stateManager.get(); }
    EngineWrapper* GetEngine() { return m_engine.get(); }
    Connection* GetConnection() { return m_connection.get(); }
    PacketDispatcher* GetDispatcher() { return m_dispatcher.get(); }
    
    static Application* Get() { return s_instance; }
    
private:
    bool CreateGameWindow();
    bool InitializeEngine();
    bool InitializeNetwork();
    void MainLoop();
    void ProcessMessages();
    void Update(float deltaTime);
    void Render();
    
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
private:
    static Application* s_instance;
    
    HINSTANCE m_hInstance = nullptr;
    HWND m_hWnd = nullptr;
    bool m_running = false;
    
    std::unique_ptr<StateManager> m_stateManager;
    std::unique_ptr<EngineWrapper> m_engine;
    std::unique_ptr<Connection> m_connection;
    std::unique_ptr<PacketDispatcher> m_dispatcher;
    std::unique_ptr<InputManager> m_input;
    
    // timing
    LARGE_INTEGER m_frequency;
    LARGE_INTEGER m_lastTime;
};

