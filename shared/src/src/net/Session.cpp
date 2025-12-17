/**
 * @file Session.cpp
 * @brief Async TCP session with ASIO
 */

#include "net/Session.h"
#include "logging/Logger.h"
#include <iostream>

namespace knc {

uint32_t Session::s_nextId = 1;

Session::Session(asio::ip::tcp::socket socket)
    : m_socket(std::move(socket))
    , m_id(s_nextId++)
{
    m_readBuffer.resize(8192);
}

Session::~Session() {
    stop();
}

void Session::start() {
    m_connected = true;
    doRead();
}

void Session::stop() {
    if (!m_connected) return;
    m_connected = false;
    
    std::error_code ec;
    m_socket.close(ec);
    
    if (m_disconnectHandler) {
        m_disconnectHandler(shared_from_this());
    }
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
    auto data = packet.serialize();
    
    // Log outgoing packet
    std::string hexDump = "CMD=0x";
    const char* hex = "0123456789ABCDEF";
    hexDump += hex[packet.cmd() >> 4];
    hexDump += hex[packet.cmd() & 0xF];
    hexDump += " Size=" + std::to_string(data.size());
    LOG_DEBUG("SESSION", "SEND to " + remoteAddress() + ": " + hexDump);
    
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
            
            // Append to receive buffer
            m_recvBuffer.insert(m_recvBuffer.end(), 
                m_readBuffer.begin(), m_readBuffer.begin() + length);
            
            // Process complete packets
            processBuffer();
            
            // Continue reading
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
            }
        }
    );
}

void Session::processBuffer() {
    while (m_recvBuffer.size() >= PACKET_HEADER_SIZE) {
        // Peek at packet size
        size_t packetSize = Packet::peekSize(m_recvBuffer.data(), m_recvBuffer.size());
        
        if (packetSize == 0 || packetSize > PACKET_MAX_SIZE) {
            // Invalid packet, disconnect
            LOG_WARN("SESSION", "Invalid packet size from " + remoteAddress());
            stop();
            return;
        }
        
        if (m_recvBuffer.size() < packetSize) {
            // Wait for more data
            break;
        }
        
        // Parse packet
        auto pkt = Packet::parse(m_recvBuffer.data(), m_recvBuffer.size());
        if (pkt && m_packetHandler) {
            m_packetHandler(shared_from_this(), *pkt);
        }
        
        // Remove processed data
        m_recvBuffer.erase(m_recvBuffer.begin(), m_recvBuffer.begin() + packetSize);
    }
}

} // namespace knc
