/// packet format is size 2 bytes cmd 1 flag 1 reserved 4 then payload

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
    // opcode is 16 bits split across cmd and flag an old uint8 constructor used to silently drop the high byte
    Packet(uint16_t cmd, uint8_t flag = 0);
    
    // factory for cmd over 255 to avoid ambiguity with the uint8 constructor
    static Packet fromCmdFull(uint16_t cmdFull);

    static std::optional<Packet> parse(const uint8_t* data, size_t len);
    static size_t peekSize(const uint8_t* data, size_t len);

    std::vector<uint8_t> serialize() const;

    uint16_t opcode() const { return m_header.opcode; }
    uint8_t cmd() const { return m_header.cmd; }
    uint8_t flag() const { return m_header.flag; }
    uint16_t cmdFull() const { return m_header.cmd | (m_header.flag << 8); }
    // read the vector not the header else a packet we BUILT always answers zero
    uint16_t payloadSize() const { return static_cast<uint16_t>(m_payload.size()); }
    size_t totalSize() const { return PACKET_HEADER_SIZE + m_payload.size(); }

    const std::vector<uint8_t>& payload() const { return m_payload; }
    std::vector<uint8_t>& payload() { return m_payload; }

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

}

