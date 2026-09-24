// a loopback server that answers the client wire with sample rows so any screen opens with no server
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "net/Packet.h"

namespace KnC::Client {

class SampleServer {
public:
    ~SampleServer();
    // listens on a free loopback port and serves one client at a time on its own thread
    bool start(const std::string& state);
    void stop();
    uint16_t port() const { return m_port; }
    bool running() const { return m_running; }

private:
    void serve();
    void handle(uint16_t opcode, ::knc::Packet& pkt);
    void tick();
    void send(const ::knc::Packet& pkt);
    void sendBurst();
    void sendProfile(uint16_t opcode);
    void sendLobby();
    void sendRoom();
    void sendGrid();
    void sendGridRows();
    void sendResult();
    void sendMissionMenu();
    void sendSocial();

    // one 0x0087 row of the sample the id kind goal limit fee gold exp and reward
    struct MissionRow {
        uint32_t id;
        uint32_t kind;
        int32_t goal;
        int32_t limitMs;
        uint32_t fee;
        uint32_t gold;
        uint32_t exp;
        uint32_t itemType;
        uint32_t itemKey;
        const char* world;
    };
    static const MissionRow kMissions[5];

    std::string m_state;
    SOCKET m_listen = INVALID_SOCKET;
    SOCKET m_client = INVALID_SOCKET;
    uint16_t m_port = 0;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_quit{false};
    std::vector<uint8_t> m_rx;
    // the second connection is the game server of the redirect
    int m_connections = 0;
    bool m_inRoom = false;
    bool m_racing = false;
    double m_goAt = -1.0;
    double m_gridAt = -1.0;
    double m_clock = 0.0;
    bool m_resultSent = false;
    // the durability of the selected kart and the uses left on the repair scroll
    int32_t m_kartDurability = 180;
    uint32_t m_scrollLeft = 4;
    uint32_t m_gold = 12850;
    uint32_t m_astro = 300;
    // the worn pendant and the mission rows the sample holds the first one cleared
    uint32_t m_pendant = 6;
    bool m_missionCleared[5] = {true, false, false, false, false};
    bool m_missionDefsSent = false;
    uint32_t m_missionRun = 99;
};

}
