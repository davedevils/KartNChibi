/**
 * @file Packet.h
 * @brief KnC packet parsing and serialization
 * 
 * Packet format:
 * [Size:2][CMD:1][Flag:1][Reserved:4][Payload:N]
 */

#pragma once
#include "Protocol.h"
#include <vector>
#include <cstring>
#include <optional>
#include <string>

namespace knc {

class Packet {
public:
    Packet() = default;
    Packet(uint8_t cmd, uint8_t flag = 0);
    
    // Parsing
    static std::optional<Packet> parse(const uint8_t* data, size_t len);
    static size_t peekSize(const uint8_t* data, size_t len);
    
    // Serialization
    std::vector<uint8_t> serialize() const;
    
    // Header access
    uint8_t cmd() const { return m_header.cmd; }
    uint8_t flag() const { return m_header.flag; }
    uint16_t payloadSize() const { return m_header.size; }
    size_t totalSize() const { return PACKET_HEADER_SIZE + m_payload.size(); }
    
    // Payload access
    const std::vector<uint8_t>& payload() const { return m_payload; }
    std::vector<uint8_t>& payload() { return m_payload; }
    
    // Payload builders
    Packet& writeInt8(int8_t val);
    Packet& writeUInt8(uint8_t val);
    Packet& writeInt16(int16_t val);
    Packet& writeUInt16(uint16_t val);
    Packet& writeInt32(int32_t val);
    Packet& writeUInt32(uint32_t val);
    Packet& writeFloat(float val);
    Packet& writeBytes(const uint8_t* data, size_t len);
    Packet& writeString(const std::string& str);
    Packet& writeWString(const std::u16string& str);
    
    // Payload readers (with position tracking)
    int8_t readInt8();
    uint8_t readUInt8();
    int16_t readInt16();
    uint16_t readUInt16();
    int32_t readInt32();
    uint32_t readUInt32();
    float readFloat();
    std::vector<uint8_t> readBytes(size_t len);
    std::string readString(size_t maxLen = 256);
    std::u16string readWString(size_t maxChars = 128);
    
    void resetReadPos() { m_readPos = 0; }
    size_t readPos() const { return m_readPos; }
    size_t remaining() const { return m_payload.size() - m_readPos; }

private:
    PacketHeader m_header{};
    std::vector<uint8_t> m_payload;
    size_t m_readPos = 0;
};

} // namespace knc

