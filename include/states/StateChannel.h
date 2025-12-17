// StateChannel.h - channel selection screen state
#pragma once

#include "states/IGameState.h"
#include <vector>
#include <string>

struct ChannelInfo {
    int32_t id;
    std::wstring name;
    int32_t currentPlayers;
    int32_t maxPlayers;
    int32_t status;
};

class StateChannel : public IGameState {
public:
    StateChannel();
    ~StateChannel() override;
    
    bool Initialize() override;
    void Cleanup() override;
    
    void OnEnter() override;
    void OnExit() override;
    
    void Update(float deltaTime) override;
    void Render() override;
    
    void OnKeyDown(int key) override;
    
    // called by packet handlers
    void SetChannelList(const std::vector<ChannelInfo>& channels);
    void OnServerRedirect(const std::string& ip, int port);
    
private:
    void SelectChannel(int index);
    
    std::vector<ChannelInfo> m_channels;
    int m_selectedIndex = 0;
    bool m_waitingForRedirect = false;
};


