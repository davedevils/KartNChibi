/// async TCP session with ASIO

#pragma once

#include <atomic>
#include <asio.hpp>
#include <chrono>
#include <memory>
#include <vector>
#include <queue>
#include <functional>
#include "Packet.h"

namespace knc {

class Session : public std::enable_shared_from_this<Session> {
public:
    using Ptr = std::shared_ptr<Session>;
    using PacketHandler = std::function<void(Session::Ptr, Packet&)>;
    using DisconnectHandler = std::function<void(Session::Ptr)>;
    
    Session(asio::ip::tcp::socket socket);
    ~Session();
    
    void start();
    /// closes the socket now the disconnect callback runs later on the loop never inside a handler
    void stop();
    /// closes once queued frames are on the wire for a refusal text stop cancels the write instead
    void closeAfterSend();
    
    void send(const Packet& packet);
    void send(const std::vector<uint8_t>& data);
    
    void setPacketHandler(PacketHandler handler) { m_packetHandler = handler; }
    void setDisconnectHandler(DisconnectHandler handler) { m_disconnectHandler = handler; }
    /// a socket that sends no byte for this long is closed zero keeps it open forever
    void setIdleTimeout(std::chrono::milliseconds limit) { m_idleLimit = limit; }

    uint32_t id() const { return m_id; }
    std::string remoteAddress() const;
    uint16_t remotePort() const;
    bool isConnected() const { return m_connected; }

    uint32_t accountId = 0;
    uint32_t characterId = 0;
    uint32_t roomId = 0;
    std::string sessionToken;
    std::string authenticatedUser;  // Username from valid launcher login
    std::u16string characterName;
    // clan tag empty without a clan rides inside the printed name so lookups still match on characterName
    std::u16string clanTag;
    /// 0 is a normal player anything above opens the gm commands
    uint8_t gmLevel = 0;
    /// while set send drops everything outside the login burst turned off once the burst ends
    bool m_burstFilter = false;
    /// unix seconds 0 when not muted a mute costs no disconnect
    int64_t mutedUntil = 0;
    /// client option 11 reported on C2S 0x0130 nonzero means the player drops every room invite
    std::atomic<bool> inviteOptOut{false};

    /// name as seen by others including tag must fit twelve characters so a long tag shortens the name first
    static constexpr size_t kMaxNameChars = 12;

    std::u16string displayName() const {
        if (clanTag.empty()) return characterName;
        const size_t deco = clanTag.size() + 2;
        if (deco >= kMaxNameChars) return characterName;   // never ship a bare tag
        std::u16string out;
        out.reserve(kMaxNameChars);
        out += u'[';
        out += clanTag;
        out += u']';
        out += characterName.substr(0, kMaxNameChars - deco);
        return out;
    }
    bool launcherAuthenticated = false;  // True if validated via launcher
    
    enum class HandshakeState {
        Initial, 
        AwaitingAuth, 
        AwaitingCharacterCreation,  // No character - waiting for creation packet
        ChannelListSent, 
        Redirected 
    };
    HandshakeState handshakeState = HandshakeState::Initial;
    bool catalogsSent = false;   ///< catalogsSent stops a repeat 0xBE catalogue send missionDefsSent stops a repeat 0x87 blind append
    bool missionDefsSent = false;

private:
    /// hands the disconnect callback to the loop so a kick under a lock cannot re enter it
    void postDisconnect();
    /// runs the packet handler and swallows a throw returns false when the session was dropped
    bool dispatch(Packet& pkt);
    void doRead();
    void doWrite();
    /// every 8 byte header frame in the buffer goes to the handler a partial one waits
    void parseFrames();
    void armIdleTimer();

    asio::ip::tcp::socket m_socket;
    asio::steady_timer m_idleTimer;
    std::chrono::milliseconds m_idleLimit{0};
    uint32_t m_id;
    bool m_connected = false;
    bool m_closeWhenDrained = false;   ///< closeAfterSend armed doWrite stops once the queue empties

    std::vector<uint8_t> m_readBuffer;
    std::vector<uint8_t> m_recvBuffer;
    std::queue<std::vector<uint8_t>> m_writeQueue;
    bool m_writing = false;

    PacketHandler m_packetHandler;
    DisconnectHandler m_disconnectHandler;

    static uint32_t s_nextId;
};

}

