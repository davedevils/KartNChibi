// StateLobby.h - lobby screen state
#pragma once

#include "states/IGameState.h"
#include <vector>
#include <string>

struct RoomInfo {
    int32_t id;
    std::wstring name;
    std::wstring hostName;
    int32_t currentPlayers;
    int32_t maxPlayers;
    int32_t mapId;
    int32_t mode;
    bool hasPassword;
    bool isPlaying;
};

class StateLobby : public IGameState {
public:
    StateLobby();
    ~StateLobby() override;
    
    bool Initialize() override;
    void Cleanup() override;
    
    void OnEnter() override;
    void OnExit() override;
    
    void Update(float deltaTime) override;
    void Render() override;
    
    void OnKeyDown(int key) override;
    
    // called by packet handlers
    void SetRoomList(const std::vector<RoomInfo>& rooms);
    void OnRoomJoined(int roomId);
    
private:
    void RefreshRoomList();
    void JoinRoom(int index);
    void CreateRoom();
    
    std::vector<RoomInfo> m_rooms;
    int m_selectedIndex = 0;
    float m_refreshTimer = 0.0f;
};


