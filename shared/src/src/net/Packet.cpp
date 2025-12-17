/**
 * @file Packet.cpp
 * @brief KnC packet parsing and serialization
 * @see docs/packets/ for protocol documentation
 */

#include "net/Packet.h"
#include <cstring>
#include <algorithm>

namespace knc {

Packet::Packet(uint8_t cmd, uint8_t flag) {
    m_header.cmd = cmd;
    m_header.flag = flag;
    m_header.size = 0;
    m_header.reserved = 0;
}

Packet Packet::fromCmdFull(uint16_t cmdFull) {
    Packet pkt;
    pkt.m_header.cmd = cmdFull & 0xFF;
    pkt.m_header.flag = (cmdFull >> 8) & 0xFF;
    pkt.m_header.size = 0;
    pkt.m_header.reserved = 0;
    return pkt;
}

std::optional<Packet> Packet::parse(const uint8_t* data, size_t len) {
    if (len < PACKET_HEADER_SIZE) {
        return std::nullopt;
    }
    
    Packet pkt;
    std::memcpy(&pkt.m_header, data, sizeof(PacketHeader));
    
    // Size field = payload size directly
    size_t payloadSize = pkt.m_header.size;
    size_t totalSize = PACKET_HEADER_SIZE + payloadSize;
    
    if (len < totalSize) {
        return std::nullopt;
    }
    
    // Copy payload
    if (payloadSize > 0) {
        pkt.m_payload.resize(payloadSize);
        std::memcpy(pkt.m_payload.data(), data + PACKET_HEADER_SIZE, payloadSize);
    }
    
    return pkt;
}

size_t Packet::peekSize(const uint8_t* data, size_t len) {
    if (len < 2) return 0;
    uint16_t size = data[0] | (data[1] << 8);
    return PACKET_HEADER_SIZE + size;
}

std::vector<uint8_t> Packet::serialize() const {
    std::vector<uint8_t> result(PACKET_HEADER_SIZE + m_payload.size());
    
    PacketHeader hdr = m_header;
    hdr.size = static_cast<uint16_t>(m_payload.size());
    
    std::memcpy(result.data(), &hdr, sizeof(PacketHeader));
    if (!m_payload.empty()) {
        std::memcpy(result.data() + PACKET_HEADER_SIZE, m_payload.data(), m_payload.size());
    }
    
    return result;
}

// Writers
Packet& Packet::writeInt8(int8_t val) {
    m_payload.push_back(static_cast<uint8_t>(val));
    return *this;
}

Packet& Packet::writeUInt8(uint8_t val) {
    m_payload.push_back(val);
    return *this;
}

Packet& Packet::writeInt16(int16_t val) {
    m_payload.push_back(val & 0xFF);
    m_payload.push_back((val >> 8) & 0xFF);
    return *this;
}

Packet& Packet::writeUInt16(uint16_t val) {
    m_payload.push_back(val & 0xFF);
    m_payload.push_back((val >> 8) & 0xFF);
    return *this;
}

Packet& Packet::writeInt32(int32_t val) {
    m_payload.push_back(val & 0xFF);
    m_payload.push_back((val >> 8) & 0xFF);
    m_payload.push_back((val >> 16) & 0xFF);
    m_payload.push_back((val >> 24) & 0xFF);
    return *this;
}

Packet& Packet::writeUInt32(uint32_t val) {
    m_payload.push_back(val & 0xFF);
    m_payload.push_back((val >> 8) & 0xFF);
    m_payload.push_back((val >> 16) & 0xFF);
    m_payload.push_back((val >> 24) & 0xFF);
    return *this;
}

Packet& Packet::writeFloat(float val) {
    uint32_t bits;
    std::memcpy(&bits, &val, sizeof(float));
    return writeUInt32(bits);
}

Packet& Packet::writeBytes(const uint8_t* data, size_t len) {
    m_payload.insert(m_payload.end(), data, data + len);
    return *this;
}

Packet& Packet::writeString(const std::string& str) {
    for (char c : str) {
        m_payload.push_back(static_cast<uint8_t>(c));
    }
    m_payload.push_back(0);  // Null terminator
    return *this;
}

Packet& Packet::writeWString(const std::u16string& str) {
    for (char16_t c : str) {
        m_payload.push_back(c & 0xFF);
        m_payload.push_back((c >> 8) & 0xFF);
    }
    m_payload.push_back(0);
    m_payload.push_back(0);
    return *this;
}

// Readers
int8_t Packet::readInt8() {
    if (m_readPos >= m_payload.size()) return 0;
    return static_cast<int8_t>(m_payload[m_readPos++]);
}

uint8_t Packet::readUInt8() {
    if (m_readPos >= m_payload.size()) return 0;
    return m_payload[m_readPos++];
}

int16_t Packet::readInt16() {
    if (m_readPos + 2 > m_payload.size()) return 0;
    int16_t val = m_payload[m_readPos] | (m_payload[m_readPos + 1] << 8);
    m_readPos += 2;
    return val;
}

uint16_t Packet::readUInt16() {
    if (m_readPos + 2 > m_payload.size()) return 0;
    uint16_t val = m_payload[m_readPos] | (m_payload[m_readPos + 1] << 8);
    m_readPos += 2;
    return val;
}

int32_t Packet::readInt32() {
    if (m_readPos + 4 > m_payload.size()) return 0;
    int32_t val = m_payload[m_readPos] |
                  (m_payload[m_readPos + 1] << 8) |
                  (m_payload[m_readPos + 2] << 16) |
                  (m_payload[m_readPos + 3] << 24);
    m_readPos += 4;
    return val;
}

uint32_t Packet::readUInt32() {
    return static_cast<uint32_t>(readInt32());
}

float Packet::readFloat() {
    uint32_t bits = readUInt32();
    float val;
    std::memcpy(&val, &bits, sizeof(float));
    return val;
}

std::vector<uint8_t> Packet::readBytes(size_t len) {
    std::vector<uint8_t> result;
    size_t toRead = std::min(len, m_payload.size() - m_readPos);
    result.assign(m_payload.begin() + m_readPos, m_payload.begin() + m_readPos + toRead);
    m_readPos += toRead;
    return result;
}

std::string Packet::readString(size_t maxLen) {
    std::string result;
    while (m_readPos < m_payload.size() && result.size() < maxLen) {
        char c = static_cast<char>(m_payload[m_readPos++]);
        if (c == '\0') break;
        result += c;
    }
    return result;
}

std::u16string Packet::readWString(size_t maxChars) {
    std::u16string result;
    while (m_readPos + 1 < m_payload.size() && result.size() < maxChars) {
        char16_t c = m_payload[m_readPos] | (m_payload[m_readPos + 1] << 8);
        m_readPos += 2;
        if (c == 0) break;
        result += c;
    }
    return result;
}

} // namespace knc
