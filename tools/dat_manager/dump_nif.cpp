// dump_nif.cpp - Dump NIF structure with names, transforms, and animations
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>

// Read helpers
uint8_t ReadU8(const uint8_t* d, size_t& o) { return d[o++]; }
uint16_t ReadU16(const uint8_t* d, size_t& o) { uint16_t v = *(uint16_t*)(d+o); o+=2; return v; }
uint32_t ReadU32(const uint8_t* d, size_t& o) { uint32_t v = *(uint32_t*)(d+o); o+=4; return v; }
float ReadF32(const uint8_t* d, size_t& o) { float v = *(float*)(d+o); o+=4; return v; }

std::string ReadString(const uint8_t* d, size_t& o) {
    uint32_t len = ReadU32(d, o);
    if (len == 0 || len > 500) return "";
    std::string s((char*)(d+o), len);
    o += len;
    return s;
}

struct NifObject {
    int index;
    std::string type;
    std::string name;
    float tx, ty, tz;  // translate
    float scale;
    std::string texture;
};

int main() {
    FILE* f = fopen("D:/KnC/KNC DEV/@git/DevClient/Data/Public/Title/INTRO.nif", "rb");
    if (!f) { printf("Cannot open file\n"); return 1; }
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = new uint8_t[size];
    fread(data, 1, size, f);
    fclose(f);
    
    printf("=== INTRO.nif Structure Dump ===\n\n");
    
    // Parse header
    size_t off = 0;
    while (off < 100 && data[off] != '\n') off++;
    off++; // skip newline
    
    uint32_t version = ReadU32(data, off);
    uint32_t userVer = ReadU32(data, off);
    uint32_t numObjects = ReadU32(data, off);
    
    printf("Version: %08X, Objects: %d\n\n", version, numObjects);
    
    // RTTI table
    uint16_t numTypes = ReadU16(data, off);
    std::vector<std::string> types;
    printf("Types (%d):\n", numTypes);
    for (int i = 0; i < numTypes; i++) {
        std::string t = ReadString(data, off);
        types.push_back(t);
        printf("  [%d] %s\n", i, t.c_str());
    }
    
    // Object type indices
    std::vector<uint16_t> objTypes;
    for (uint32_t i = 0; i < numObjects; i++) {
        objTypes.push_back(ReadU16(data, off));
    }
    
    // Skip groups
    uint32_t numGroups = ReadU32(data, off);
    printf("\nGroups: %d\n", numGroups);
    
    printf("\nData starts at offset: %zu\n", off);
    printf("\n=== Objects with Names and Transforms ===\n\n");
    
    // Now we need to parse each object to find names and transforms
    // This is complex because object sizes vary by type
    // For now, scan for name patterns followed by transform data
    
    for (size_t scan = off; scan < size - 100; scan++) {
        // Look for string length that makes sense (4-30 chars)
        uint32_t len = *(uint32_t*)(data + scan);
        if (len >= 4 && len <= 30) {
            // Check if it's readable ASCII
            bool readable = true;
            for (uint32_t i = 0; i < len && i < 30; i++) {
                char c = data[scan + 4 + i];
                if (c < 32 || c > 126) { readable = false; break; }
            }
            
            if (readable) {
                std::string name((char*)(data + scan + 4), len);
                
                // Check if name looks like an object name (Plane, Dummy, etc)
                if (name.find("Plane") == 0 || name.find("Dummy") == 0 || 
                    name.find("Scene") == 0 || name.find("pum_") == 0 ||
                    name.find("prin_") == 0 || name.find("fire") == 0 ||
                    name.find("back") == 0 || name.find("yok") == 0) {
                    
                    // After name, look for transform data
                    // Format: extra data refs, controller ref, flags, translate, rotate, scale
                    size_t dataOff = scan + 4 + len;
                    
                    // Skip a few bytes and look for floats
                    for (int skip = 0; skip < 50; skip += 4) {
                        float x = *(float*)(data + dataOff + skip);
                        float y = *(float*)(data + dataOff + skip + 4);
                        float z = *(float*)(data + dataOff + skip + 8);
                        
                        // Check if these look like reasonable coordinates
                        if (x > -2000 && x < 2000 && y > -2000 && y < 2000 && z > -500 && z < 500) {
                            if (x != 0 || y != 0 || z != 0) {
                                if (!(x == 1.0f && y == 0 && z == 0) && 
                                    !(x == 0 && y == 1.0f && z == 0)) {
                                    printf("%-20s pos=(%.0f, %.0f, %.0f) @%zu+%d\n", 
                                           name.c_str(), x, y, z, scan, skip);
                                    break;
                                }
                            }
                        }
                    }
                    
                    // Skip past this name to avoid re-matching
                    scan += len + 3;
                }
            }
        }
    }
    
    delete[] data;
    printf("\nDone!\n");
    return 0;
}

