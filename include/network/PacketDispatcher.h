// PacketDispatcher.h - packet routing and handler registration
#pragma once

#include "network/Packet.h"
#include <functional>
#include <unordered_map>
#include <cstdio>

class PacketDispatcher {
public:
    using Handler = std::function<void(Packet&)>;
    
    void Initialize();
    void RegisterHandler(uint16_t cmd, Handler handler);
    void Dispatch(Packet& packet);
    
private:
    // key = cmd for basic, or fullCmd for extended
    std::unordered_map<uint16_t, Handler> m_handlers;
};


