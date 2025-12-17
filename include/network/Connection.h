// Connection.h - TCP connection manager
#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <vector>

class Packet;

class Connection {
public:
    Connection();
    ~Connection();
    
    bool Connect(const std::string& host, int port);
    void Disconnect();
    bool IsConnected() const { return m_connected; }
    
    bool Send(const Packet& packet);
    bool Receive(Packet& packet);
    
    void SetNonBlocking(bool nonBlocking);
    
private:
    SOCKET m_socket = INVALID_SOCKET;
    bool m_connected = false;
    std::vector<uint8_t> m_recvBuffer;
};

