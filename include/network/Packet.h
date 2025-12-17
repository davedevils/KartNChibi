// Packet.h - network packet handling
#pragma once

#include "Types.h"
#include <vector>
#include <string>

class Packet {
public:
    // construction
    Packet();
    static Packet Create(uint8_t cmd, uint8_t flag = 0);
    static Packet FromBuffer(const uint8_t* data, size_t size);
    
    // header access
    uint16_t GetSize() const { return m_header.size; }
    uint8_t GetCmd() const { return m_header.cmd; }
    uint8_t GetFlag() const { return m_header.flag; }
    uint16_t GetFullCmd() const { return m_header.cmd | (m_header.flag << 8); }
    
    // read operations
    int8_t ReadInt8();
    int16_t ReadInt16();
    int32_t ReadInt32();
    float ReadFloat();
    std::string ReadString();
    std::wstring ReadWString();
    void ReadBytes(void* dest, size_t count);
    
    // write operations
    void WriteInt8(int8_t value);
    void WriteInt16(int16_t value);
    void WriteInt32(int32_t value);
    void WriteFloat(float value);
    void WriteString(const std::string& str);
    void WriteWString(const std::wstring& str);
    void WriteBytes(const void* src, size_t count);
    
    // finalization
    void Finalize();
    const uint8_t* GetData() const;
    size_t GetTotalSize() const;
    
    // utility
    void Reset();
    bool HasData() const { return m_readPos < m_payload.size(); }
    
private:
    PacketHeader m_header;
    std::vector<uint8_t> m_payload;
    size_t m_readPos = 0;
};

