#pragma once
// PakReader.h - Custom PAK archive reader (no EngineDLL dependency)

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace knc {

struct PakEntry {
    std::string name;
    uint32_t offset;
    uint32_t size;
    uint32_t compressedSize;
    bool compressed;
};

class PakReader {
public:
    PakReader() = default;
    ~PakReader();
    
    // Open a PAK file
    bool Open(const std::string& path);
    void Close();
    
    // Check if file exists in PAK
    bool Exists(const std::string& path) const;
    
    // Read file from PAK (returns uncompressed data)
    std::vector<uint8_t> ReadFile(const std::string& path) const;
    
    // Get file size
    uint32_t GetFileSize(const std::string& path) const;
    
    // List all files
    std::vector<std::string> ListFiles() const;
    
private:
    bool ParseHeader();
    bool ParseFileTable();
    std::vector<uint8_t> Decompress(const uint8_t* data, uint32_t compSize, uint32_t uncompSize) const;
    
    FILE* m_file = nullptr;
    std::string m_path;
    std::unordered_map<std::string, PakEntry> m_entries;
    
    // PAK header info
    uint32_t m_version = 0;
    uint32_t m_fileCount = 0;
    uint32_t m_tableOffset = 0;
};

} // namespace knc

