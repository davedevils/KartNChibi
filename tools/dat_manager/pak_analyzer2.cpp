/**
 * NKZIP PAK format deep analyzer
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>

#pragma pack(push, 1)
struct NKZIPHeader {
    char magic[8];      // "NKZIP\0\0\0"
    uint32_t unknown1;  // 
    uint32_t unknown2;  // 
    uint32_t version;   // 0x31 = '1' ?
    uint32_t unknown3[3];
    uint32_t unknown4;  // compressed size?
    uint32_t unknown5;  // offset?
    uint32_t pathLen;   // length of first path?
};
#pragma pack(pop)

int main() {
    const char* pakPath = "D:/KnC/KNC DEV/@git/DevClient/pak001.dat";
    
    FILE* f = fopen(pakPath, "rb");
    if (!f) {
        printf("Cannot open %s\n", pakPath);
        return 1;
    }
    
    printf("=== NKZIP Deep Analyzer ===\n\n");
    
    // Read header area
    uint8_t data[512];
    fread(data, 1, 512, f);
    
    // Magic is "NKZIP"
    printf("Magic: %.5s\n", data);
    
    // After magic (offset 8), there's some data
    // At offset 0x20 (32), we see the first path starting with "./"
    
    // Let's find where file entries start
    // Looking at the hex dump, at offset 0x28 we have "./Data/Car/basic_1.c"
    
    printf("\n=== Looking for file table structure ===\n");
    
    // The structure seems to be:
    // [compressed_size:4][offset:4][path_len:4][path:path_len]
    
    // Let's read from offset 32
    fseek(f, 32, SEEK_SET);
    
    struct FileEntry {
        uint32_t compressedSize;
        uint32_t offset;
        uint32_t pathLen;
        std::string path;
    };
    
    std::vector<FileEntry> entries;
    
    // Try to parse entries
    for (int i = 0; i < 50; i++) {
        long pos = ftell(f);
        
        FileEntry entry;
        fread(&entry.compressedSize, 4, 1, f);
        fread(&entry.offset, 4, 1, f);
        fread(&entry.pathLen, 4, 1, f);
        
        // Sanity check
        if (entry.pathLen > 500 || entry.pathLen == 0) {
            printf("Invalid entry at 0x%lX (pathLen=%u), stopping\n", pos, entry.pathLen);
            break;
        }
        
        char pathBuf[512] = {0};
        fread(pathBuf, 1, entry.pathLen, f);
        entry.path = pathBuf;
        
        // Check if path looks valid
        if (entry.path.find("./") != 0 && entry.path.find("Data") == std::string::npos) {
            printf("Invalid path at 0x%lX: '%s', stopping\n", pos, entry.path.c_str());
            break;
        }
        
        printf("[%3d] Size: %8u  Offset: 0x%08X  Path: %s\n", 
               i, entry.compressedSize, entry.offset, entry.path.c_str());
        
        entries.push_back(entry);
    }
    
    printf("\n=== Found %zu entries ===\n", entries.size());
    
    // Try to find texture entries
    printf("\n=== Searching for texture files ===\n");
    fseek(f, 32, SEEK_SET);
    
    int texCount = 0;
    long pos = 32;
    while (pos < 50 * 1024 * 1024) { // Search first 50MB of file table
        fseek(f, pos, SEEK_SET);
        
        uint32_t sizes[3];
        if (fread(sizes, 4, 3, f) != 3) break;
        
        uint32_t pathLen = sizes[2];
        if (pathLen > 500 || pathLen == 0) {
            pos++;
            continue;
        }
        
        char pathBuf[512] = {0};
        fread(pathBuf, 1, pathLen, f);
        
        std::string path = pathBuf;
        if (path.find("./Data/") == 0 || path.find("Data/") == 0) {
            // Check for interesting files
            if (path.find(".dds") != std::string::npos || 
                path.find(".DDS") != std::string::npos) {
                if (texCount < 30) {
                    printf("Texture: %s (size=%u, offset=0x%X)\n", 
                           path.c_str(), sizes[0], sizes[1]);
                }
                texCount++;
            }
            pos = ftell(f);
        } else {
            pos++;
        }
    }
    
    printf("\nTotal textures found: %d\n", texCount);
    
    fclose(f);
    return 0;
}

