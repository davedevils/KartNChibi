
#include "net/Session.h"
#include "logging/Logger.h"
#include <cstring>
#include <iostream>
#include <string>

namespace knc {

uint32_t Session::s_nextId = 1;

namespace {
// four hex digits so a dispatch error names the real opcode not a truncated byte
std::string opcodeHex(uint16_t v) {
    const char* hex = "0123456789ABCDEF";
    char buf[5] = { hex[(v >> 12) & 0xF], hex[(v >> 8) & 0xF],
                    hex[(v >> 4) & 0xF], hex[v & 0xF], 0 };
    return std::string(buf, 4);
}
}

Session::Session(asio::ip::tcp::socket socket)
    : m_socket(std::move(socket))
    , m_idleTimer(m_socket.get_executor())
    , m_id(s_nextId++)
{
    m_readBuffer.resize(8192);
}

Session::~Session() {
    stop();
}

void Session::start() {
    m_connected = true;
    armIdleTimer();
    doRead();
}

void Session::stop() {
    if (!m_connected) return;
    m_connected = false;

    std::error_code ec;
    m_socket.close(ec);
    m_idleTimer.cancel();

    postDisconnect();
}

void Session::postDisconnect() {
    if (!m_disconnectHandler) return;

    // a kick inside a handler holds a lock the disconnect path takes again so it goes async
    auto self = weak_from_this().lock();
    if (!self) return;   // the destructor is running nobody may own this again

    auto handler = m_disconnectHandler;
    asio::post(m_socket.get_executor(), [self, handler]() {
        try {
            handler(self);
        } catch (const std::exception& e) {
            LOG_ERROR("SESSION", std::string("disconnect handler threw: ") + e.what());
        } catch (...) {
            LOG_ERROR("SESSION", "disconnect handler threw an unknown error");
        }
    });
}

bool Session::dispatch(Packet& pkt) {
    if (!m_packetHandler) return true;
    try {
        m_packetHandler(shared_from_this(), pkt);
    } catch (const std::exception& e) {
        LOG_ERROR("SESSION", "handler threw on opcode 0x" + opcodeHex(pkt.opcode()) +
                             " from " + remoteAddress() + ": " + e.what());
        stop();
        return false;
    } catch (...) {
        LOG_ERROR("SESSION", "handler threw an unknown error on opcode 0x" +
                             opcodeHex(pkt.opcode()) + " from " + remoteAddress());
        stop();
        return false;
    }
    return m_connected;
}

void Session::closeAfterSend() {
    if (!m_connected) return;
    if (m_writeQueue.empty()) { stop(); return; }
    m_closeWhenDrained = true;
}

std::string Session::remoteAddress() const {
    try {
        return m_socket.remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

uint16_t Session::remotePort() const {
    try {
        return m_socket.remote_endpoint().port();
    } catch (...) {
        return 0;
    }
}

void Session::send(const Packet& packet) {
    // KNC MINIMAL BURST reproduces the login burst 0xBE then prices characters karts and parts and lifts once the client acks
    if (m_burstFilter) {
        static const uint16_t keep[] = { 0x00BE, 0x00C6, 0x00BF, 0x00C0, 0x00C2 };
        bool allowed = false;
        for (uint16_t k : keep) if (k == packet.opcode()) { allowed = true; break; }
        if (!allowed) return;
    }

    auto data = packet.serialize();
    LOG_DEBUG("SESSION", "SEND to " + remoteAddress() + ": CMD=0x" + opcodeHex(packet.opcode()) +
              " Size=" + std::to_string(data.size()));
    send(data);
}

void Session::send(const std::vector<uint8_t>& data) {
    if (!m_connected) return;

    bool wasEmpty = m_writeQueue.empty();
    m_writeQueue.push(data);

    if (wasEmpty) {
        doWrite();
    }
}

void Session::armIdleTimer() {
    if (m_idleLimit.count() <= 0 || !m_connected) return;
    // a new expiry aborts the wait before so only a silent socket reaches the close
    m_idleTimer.expires_after(m_idleLimit);
    std::weak_ptr<Session> weak = weak_from_this();
    m_idleTimer.async_wait([weak](const std::error_code& ec) {
        if (ec) return;
        auto self = weak.lock();
        if (!self || !self->m_connected) return;
        LOG_WARN("SESSION", "no byte from " + self->remoteAddress() + " for " +
                 std::to_string(self->m_idleLimit.count() / 1000) + " s, closing");
        self->stop();
    });
}

void Session::doRead() {
    if (!m_connected) return;
    
    auto self = shared_from_this();
    m_socket.async_read_some(
        asio::buffer(m_readBuffer),
        [this, self](std::error_code ec, std::size_t length) {
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    stop();
                }
                return;
            }
            
            m_recvBuffer.insert(m_recvBuffer.end(),
                m_readBuffer.begin(), m_readBuffer.begin() + length);

            armIdleTimer();
            parseFrames();

            doRead();
        }
    );
}

void Session::doWrite() {
    if (!m_connected || m_writeQueue.empty()) return;
    
    auto self = shared_from_this();
    asio::async_write(
        m_socket,
        asio::buffer(m_writeQueue.front()),
        [this, self](std::error_code ec, std::size_t /*length*/) {
            if (ec) {
                stop();
                return;
            }
            
            m_writeQueue.pop();
            
            if (!m_writeQueue.empty()) {
                doWrite();
            } else if (m_closeWhenDrained) {
                // the last frame is out a refusal text reached the client before the drop
                stop();
            }
        }
    );
}

// the stock client speaks only the 8 byte header so a first byte 0x4B is a size byte
void Session::parseFrames() {
    while (m_connected && m_recvBuffer.size() >= PACKET_HEADER_SIZE) {
        size_t packetSize = Packet::peekSize(m_recvBuffer.data(), m_recvBuffer.size());

        // a header past the C2S cap would hold up to 64 KB per socket before any check ran
        if (packetSize == 0 || packetSize > PACKET_HEADER_SIZE + PACKET_MAX_C2S_PAYLOAD) {
            LOG_WARN("SESSION", "Invalid packet size " + std::to_string(packetSize) + " from " + remoteAddress());
            stop();
            return;
        }

        if (m_recvBuffer.size() < packetSize) {
            break;
        }

        auto pkt = Packet::parse(m_recvBuffer.data(), m_recvBuffer.size());
        if (pkt && !dispatch(*pkt)) return;   // this client is gone the loop keeps running

        m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + packetSize);
    }
}

} // namespace knc
