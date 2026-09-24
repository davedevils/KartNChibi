/// packet header is size uint16 cmd uint8 flag uint8 and 4 reserved bytes followed by variable payload

#pragma once

#include "../core/Types.h"
#include <cstring>

namespace KnC {
namespace Net {

#pragma pack(push, 1)
struct PacketHeader {
    uint16 size;        // payload size excludes header
    uint8  cmd;
    uint8  flag;        // sub command or variant
    uint8  reserved[4];
    
    PacketHeader() 
        : size(0), cmd(0), flag(0), reserved{0, 0, 0, 0} {}
    
    PacketHeader(uint8 cmd, uint8 flag = 0, uint16 size = 0)
        : size(size), cmd(cmd), flag(flag), reserved{0, 0, 0, 0} {}
    
    bool IsValid() const {
        return size <= Limits::MAX_PACKET_SIZE;
    }
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");

/// fixed size packet buffer
class PacketBuffer {
public:
    static constexpr uint32 MAX_SIZE = Limits::MAX_PACKET_SIZE;
    
    PacketBuffer() : m_size(0) {
        std::memset(m_data, 0, MAX_SIZE);
    }
    
    PacketHeader* GetHeader() {
        return reinterpret_cast<PacketHeader*>(m_data);
    }
    
    const PacketHeader* GetHeader() const {
        return reinterpret_cast<const PacketHeader*>(m_data);
    }
    
    uint8* GetPayload() {
        return m_data + sizeof(PacketHeader);
    }
    
    const uint8* GetPayload() const {
        return m_data + sizeof(PacketHeader);
    }
    
    uint16 GetPayloadSize() const {
        return GetHeader()->size;
    }
    
    uint32 GetTotalSize() const {
        return sizeof(PacketHeader) + GetPayloadSize();
    }
    
    uint8* GetData() { return m_data; }
    const uint8* GetData() const { return m_data; }
    
    void Init(uint8 cmd, uint8 flag = 0) {
        PacketHeader* header = GetHeader();
        header->cmd = cmd;
        header->flag = flag;
        header->size = 0;
        std::memset(header->reserved, 0, 4);
        m_size = sizeof(PacketHeader);
    }
    
    bool Write(const void* data, uint16 size) {
        if (m_size + size > MAX_SIZE) {
            return false;
        }
        std::memcpy(m_data + m_size, data, size);
        m_size += size;
        GetHeader()->size = m_size - sizeof(PacketHeader);
        return true;
    }
    
    template<typename T>
    bool Write(const T& value) {
        return Write(&value, sizeof(T));
    }

    // write string null terminated
    bool WriteString(const char* str) {
        uint16 len = static_cast<uint16>(std::strlen(str)) + 1;
        return Write(str, len);
    }
    
    // write string with fixed length padded with zeros
    bool WriteFixedString(const char* str, uint16 maxLen) {
        uint16 len = static_cast<uint16>(std::strlen(str));
        if (len >= maxLen) len = maxLen - 1;
        
        if (!Write(str, len)) return false;

        uint16 padding = maxLen - len;
        uint8 zero = 0;
        for (uint16 i = 0; i < padding; ++i) {
            if (!Write(zero)) return false;
        }
        return true;
    }
    
    void Clear() {
        m_size = 0;
        std::memset(m_data, 0, MAX_SIZE);
    }

private:
    uint8 m_data[MAX_SIZE];
    uint32 m_size;
};

class PacketReader {
public:
    PacketReader(const uint8* data, uint32 size) 
        : m_data(data), m_size(size), m_offset(0) {}
    
    const PacketHeader* GetHeader() const {
        return reinterpret_cast<const PacketHeader*>(m_data);
    }
    
    bool CanRead(uint32 bytes) const {
        return m_offset + bytes <= m_size;
    }
    
    template<typename T>
    bool Read(T& out) {
        if (!CanRead(sizeof(T))) {
            return false;
        }
        std::memcpy(&out, m_data + m_offset, sizeof(T));
        m_offset += sizeof(T);
        return true;
    }
    
    bool ReadBytes(void* out, uint32 size) {
        if (!CanRead(size)) {
            return false;
        }
        std::memcpy(out, m_data + m_offset, size);
        m_offset += size;
        return true;
    }
    
    // read string null terminated
    bool ReadString(char* out, uint32 maxLen) {
        uint32 start = m_offset;
        while (m_offset < m_size && m_data[m_offset] != 0) {
            if (m_offset - start >= maxLen - 1) {
                return false; // string too long
            }
            out[m_offset - start] = m_data[m_offset];
            m_offset++;
        }

        if (m_offset >= m_size) {
            return false; // no null terminator found
        }
        
        out[m_offset - start] = '\0';
        m_offset++;
        return true;
    }
    
    // read string with fixed length
    bool ReadFixedString(char* out, uint32 len) {
        if (!CanRead(len)) {
            return false;
        }
        std::memcpy(out, m_data + m_offset, len);
        m_offset += len;
        out[len] = '\0';
        return true;
    }
    
    void Skip(uint32 bytes) {
        m_offset += bytes;
    }
    
    uint32 GetOffset() const { return m_offset; }
    uint32 GetRemaining() const { return m_size - m_offset; }

private:
    const uint8* m_data;
    uint32 m_size;
    uint32 m_offset;
};

}
}

