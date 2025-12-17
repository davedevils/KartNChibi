/**
 * NKZIP PAK File Reader v3
 * Properly parses the file table at the start of PAK
 * 
 * Format discovered:
 * - 0x00: "NKZIP\0\0\0" magic
 * - 0x10: Version "1"
 * - 0x20+: File entries, each ~420 bytes:
 *   - [8 bytes header][4 bytes size][path ~400 bytes null-terminated + padding]
 *   - Followed by file data
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstring>
#include <algorithm>

namespace KnC {

struct PakEntry {
    std::string path;      // Full path like "./Data/Car/..."
    std::string filename;  // Just filename
    std::string extension;
    uint32_t offset;       // Offset of file data in PAK
    uint32_t size;
    int pakIndex;
};

struct MapInfo {
    std::string path;
    std::string category;
    std::string name;
    bool hasGeometry;
    bool hasTrack;
    bool hasSky;
};

struct PakFile {
    FILE* file;
    std::string path;
    long fileSize;
};

class PakReader {
public:
    PakReader() {}
    ~PakReader() { Close(); }
    
    bool OpenGameDir(const std::string& gameDir) {
        Close();
        m_gameDir = gameDir;
        
        if (!m_gameDir.empty() && m_gameDir.back() != '/' && m_gameDir.back() != '\\') {
            m_gameDir += "/";
        }
        
        printf("[PAK] Opening game directory: %s\n", m_gameDir.c_str());
        
        int pakCount = 0;
        for (int i = 1; i <= 9; i++) {
            char pakName[32];
            sprintf(pakName, "pak%03d.dat", i);
            std::string pakPath = m_gameDir + pakName;
            
            if (OpenSinglePak(pakPath, (int)m_pakFiles.size())) {
                pakCount++;
            }
        }
        
        if (pakCount == 0) {
            printf("[PAK] ERROR: No PAK files found!\n");
            return false;
        }
        
        printf("[PAK] Opened %d PAK files, %d total entries\n", pakCount, (int)m_entries.size());
        BuildMapList();
        return true;
    }
    
    bool Open(const std::string& pakPath) {
        Close();
        size_t lastSlash = pakPath.rfind('/');
        if (lastSlash == std::string::npos) lastSlash = pakPath.rfind('\\');
        if (lastSlash != std::string::npos) {
            m_gameDir = pakPath.substr(0, lastSlash + 1);
        }
        
        if (!OpenSinglePak(pakPath, 0)) return false;
        BuildMapList();
        return true;
    }
    
    void Close() {
        for (auto& pf : m_pakFiles) {
            if (pf.file) fclose(pf.file);
        }
        m_pakFiles.clear();
        m_entries.clear();
        m_nameIndex.clear();
        m_pathIndex.clear();
        m_maps.clear();
    }
    
    bool FileExists(const std::string& filename) {
        std::string name = GetFilename(filename);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        return m_nameIndex.find(name) != m_nameIndex.end();
    }
    
    bool PathExists(const std::string& path) {
        std::string lower = NormalizePath(path);
        return m_pathIndex.find(lower) != m_pathIndex.end();
    }
    
    std::vector<uint8_t> ReadFile(const std::string& filename) {
        std::string name = GetFilename(filename);
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        auto it = m_nameIndex.find(name);
        if (it != m_nameIndex.end()) {
            return ReadEntry(m_entries[it->second]);
        }
        return {};
    }
    
    std::vector<uint8_t> ReadPath(const std::string& path) {
        std::string lower = NormalizePath(path);
        auto it = m_pathIndex.find(lower);
        if (it != m_pathIndex.end()) {
            return ReadEntry(m_entries[it->second]);
        }
        return {};
    }
    
    size_t GetEntryCount() const { return m_entries.size(); }
    const std::vector<MapInfo>& GetMaps() const { return m_maps; }
    const std::vector<PakEntry>& GetEntries() const { return m_entries; }
    const std::string& GetGameDir() const { return m_gameDir; }
    
    // List all files in a path prefix (like a directory)
    std::vector<std::string> ListFilesInPath(const std::string& pathPrefix) {
        std::vector<std::string> result;
        std::string prefix = NormalizePath(pathPrefix);
        if (!prefix.empty() && prefix.back() != '/') prefix += '/';
        
        for (const auto& entry : m_entries) {
            std::string entryPath = NormalizePath(entry.path);
            if (entryPath.find(prefix) == 0) {
                // Get relative path after prefix
                std::string relPath = entryPath.substr(prefix.length());
                // Only include direct children (no subdirs)
                if (relPath.find('/') == std::string::npos) {
                    result.push_back(entry.path);
                }
            }
        }
        return result;
    }
    
    // List all unique subdirectories in a path
    std::vector<std::string> ListSubdirs(const std::string& pathPrefix) {
        std::set<std::string> subdirs;
        std::string prefix = NormalizePath(pathPrefix);
        if (!prefix.empty() && prefix.back() != '/') prefix += '/';
        
        for (const auto& entry : m_entries) {
            std::string entryPath = NormalizePath(entry.path);
            if (entryPath.find(prefix) == 0) {
                std::string relPath = entryPath.substr(prefix.length());
                size_t slashPos = relPath.find('/');
                if (slashPos != std::string::npos) {
                    subdirs.insert(prefix + relPath.substr(0, slashPos));
                }
            }
        }
        return std::vector<std::string>(subdirs.begin(), subdirs.end());
    }
    
private:
    std::vector<PakFile> m_pakFiles;
    std::string m_gameDir;
    std::vector<PakEntry> m_entries;
    std::map<std::string, size_t> m_nameIndex;
    std::map<std::string, size_t> m_pathIndex;
    std::vector<MapInfo> m_maps;
    
    std::string NormalizePath(const std::string& path) {
        std::string p = path;
        std::transform(p.begin(), p.end(), p.begin(), ::tolower);
        for (char& c : p) if (c == '\\') c = '/';
        // Remove leading "./"
        if (p.size() > 2 && p[0] == '.' && p[1] == '/') {
            p = p.substr(2);
        }
        return p;
    }
    
    std::string GetFilename(const std::string& path) {
        size_t pos = path.rfind('/');
        if (pos == std::string::npos) pos = path.rfind('\\');
        if (pos != std::string::npos) return path.substr(pos + 1);
        return path;
    }
    
    std::string GetExtension(const std::string& filename) {
        size_t dot = filename.rfind('.');
        if (dot != std::string::npos) {
            std::string ext = filename.substr(dot);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            return ext;
        }
        return "";
    }
    
    bool OpenSinglePak(const std::string& pakPath, int pakIndex) {
        FILE* f = fopen(pakPath.c_str(), "rb");
        if (!f) return false;
        
        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        // Check magic
        char magic[8];
        fread(magic, 1, 8, f);
        if (strncmp(magic, "NKZIP", 5) != 0) {
            fclose(f);
            return false;
        }
        
        PakFile pf;
        pf.file = f;
        pf.path = pakPath;
        pf.fileSize = fileSize;
        m_pakFiles.push_back(pf);
        
        printf("[PAK] Opened: %s (%.2f MB)\n", pakPath.c_str(), fileSize / 1024.0 / 1024.0);
        
        ScanPakEntries(pakIndex);
        return true;
    }
    
    std::vector<uint8_t> ReadEntry(const PakEntry& entry) {
        if (entry.pakIndex < 0 || entry.pakIndex >= (int)m_pakFiles.size()) {
            return {};
        }
        
        FILE* f = m_pakFiles[entry.pakIndex].file;
        std::vector<uint8_t> data(entry.size);
        fseek(f, entry.offset, SEEK_SET);
        fread(data.data(), 1, entry.size, f);
        return data;
    }
    
    void ScanPakEntries(int pakIndex) {
        if (pakIndex >= (int)m_pakFiles.size()) return;
        
        FILE* f = m_pakFiles[pakIndex].file;
        long fileSize = m_pakFiles[pakIndex].fileSize;
        
        // NKZIP format:
        // - Header: 32 bytes (0x00-0x1F)
        // - Entry: 12 bytes header + 260 bytes path area + data
        // - Entry header: [4 bytes flags][4 bytes ?][4 bytes size]
        // - Path: starts at header+12, null-terminated, padded to 260 bytes
        // - Data: starts at path_start + 260
        
        const size_t CHUNK = 8 * 1024 * 1024;  // 8MB
        std::vector<uint8_t> buffer(CHUNK);
        
        int fileCount = 0;
        std::map<std::string, int> extCounts;
        
        for (long pos = 0; pos < fileSize; pos += CHUNK - 512) {
            fseek(f, pos, SEEK_SET);
            size_t bytesRead = fread(buffer.data(), 1, CHUNK, f);
            if (bytesRead < 300) break;
            
            // Scan for "./" path patterns
            for (size_t i = 12; i < bytesRead - 300; i++) {
                if (buffer[i] == '.' && buffer[i+1] == '/') {
                    // Check if followed by valid path char (A-Z or a-z)
                    char c = buffer[i+2];
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))) continue;
                    
                    // Find end of path (null terminator)
                    size_t end = i + 2;
                    while (end < bytesRead && buffer[end] != 0 && end - i < 256) {
                        char ch = buffer[end];
                        // Valid path characters
                        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                              (ch >= '0' && ch <= '9') || ch == '/' || ch == '\\' ||
                              ch == '_' || ch == '-' || ch == '.' || ch == ' ' || 
                              ch == '(' || ch == ')' || ch == '[' || ch == ']')) {
                            break;
                        }
                        end++;
                    }
                    
                    size_t pathLen = end - i;
                    if (pathLen < 5 || pathLen > 250) continue;
                    
                    std::string path((char*)&buffer[i], pathLen);
                    
                    // Must have a file extension
                    size_t dot = path.rfind('.');
                    if (dot == std::string::npos || dot < path.size() - 6) continue;
                    
                    std::string ext = path.substr(dot);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                    
                    // Known extensions
                    bool validExt = (ext == ".nif" || ext == ".dds" || ext == ".png" || 
                                    ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
                                    ext == ".wav" || ext == ".ogg" || ext == ".mp3" ||
                                    ext == ".xml" || ext == ".txt" || ext == ".ini" ||
                                    ext == ".car" || ext == ".kf" || ext == ".kfm" ||
                                    ext == ".tga" || ext == ".lua" || ext == ".csv" ||
                                    ext == ".ifl" || ext == ".col");  // IFL = animated textures, COL = collision
                    
                    if (!validExt) continue;
                    
                    // Get size from entry header (4 bytes before path)
                    uint32_t fileSize = 0;
                    if (i >= 4) {
                        fileSize = *(uint32_t*)&buffer[i - 4];
                        // Sanity check
                        if (fileSize > 50 * 1024 * 1024) fileSize = 0;
                    }
                    
                    if (fileSize == 0) continue;  // Skip invalid entries
                    
                    // Data offset = path_start + 260 (fixed path area size)
                    long dataOffset = pos + i + 260;
                    
                    AddEntry(path, (uint32_t)dataOffset, fileSize, pakIndex);
                    
                    fileCount++;
                    extCounts[ext]++;
                    
                    // Skip past this entry (path area + data)
                    i += 260 + fileSize;
                    if (i >= bytesRead) break;
                }
            }
        }
        
        printf("[PAK] pak%03d: %d files (", pakIndex + 1, fileCount);
        bool first = true;
        for (auto& p : extCounts) {
            if (!first) printf(", ");
            printf("%d %s", p.second, p.first.c_str() + 1);
            first = false;
        }
        printf(")\n");
    }
    
    void AddEntry(const std::string& path, uint32_t offset, uint32_t size, int pakIndex) {
        PakEntry entry;
        entry.path = path;
        entry.filename = GetFilename(path);
        entry.extension = GetExtension(entry.filename);
        entry.offset = offset;
        entry.size = size;
        entry.pakIndex = pakIndex;
        
        std::string lowerPath = NormalizePath(path);
        std::string lowerName = entry.filename;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        // Avoid duplicates
        if (m_pathIndex.find(lowerPath) != m_pathIndex.end()) return;
        
        size_t idx = m_entries.size();
        m_entries.push_back(entry);
        m_pathIndex[lowerPath] = idx;
        
        if (m_nameIndex.find(lowerName) == m_nameIndex.end()) {
            m_nameIndex[lowerName] = idx;
        }
    }
    
    void BuildMapList() {
        printf("[PAK] Scanning for maps...\n");
        
        std::set<std::string> mapPaths;
        
        for (const auto& entry : m_entries) {
            std::string lower = entry.path;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            
            bool isGeometry = lower.find("geometry.nif") != std::string::npos;
            bool isTrack = lower.find("track.nif") != std::string::npos;
            bool isSky = lower.find("sky.nif") != std::string::npos;
            
            if (isGeometry || isTrack || isSky) {
                if (lower.find("world/") != std::string::npos || lower.find("world\\") != std::string::npos) {
                    size_t lastSlash = entry.path.rfind('/');
                    if (lastSlash == std::string::npos) lastSlash = entry.path.rfind('\\');
                    if (lastSlash != std::string::npos) {
                        std::string mapPath = entry.path.substr(0, lastSlash);
                        mapPaths.insert(mapPath);
                    }
                }
            }
        }
        
        for (const std::string& mapPath : mapPaths) {
            MapInfo info;
            info.path = mapPath;
            
            std::string path = mapPath;
            for (char& c : path) if (c == '\\') c = '/';
            
            size_t lastSlash = path.rfind('/');
            if (lastSlash != std::string::npos) {
                info.name = path.substr(lastSlash + 1);
                
                size_t prevSlash = path.rfind('/', lastSlash - 1);
                if (prevSlash != std::string::npos) {
                    info.category = path.substr(prevSlash + 1, lastSlash - prevSlash - 1);
                }
            }
            
            std::string baseLower = NormalizePath(mapPath);
            
            info.hasGeometry = m_pathIndex.count(baseLower + "/geometry.nif") > 0;
            info.hasTrack = m_pathIndex.count(baseLower + "/track.nif") > 0;
            info.hasSky = m_pathIndex.count(baseLower + "/sky.nif") > 0;
            
            if (info.hasGeometry || info.hasTrack) {
                m_maps.push_back(info);
            }
        }
        
        std::sort(m_maps.begin(), m_maps.end(), [](const MapInfo& a, const MapInfo& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.name < b.name;
        });
        
        printf("[PAK] Found %zu maps\n", m_maps.size());
    }
};

inline PakReader& GetPakReader() {
    static PakReader reader;
    return reader;
}

} // namespace KnC
