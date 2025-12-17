// Dump NIF header details
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

int main(int argc, char* argv[]) {
    const char* path = argc > 1 ? argv[1] : "D:/KnC/KNC DEV/@git/DevClient/Data/Public/Car/Body/High/Basic_1/BODY.nif";
    
    printf("Reading: %s\n\n", path);
    
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        printf("Failed to open file!\n");
        return 1;
    }
    
    // Read header line
    char header[256];
    file.getline(header, 256, '\n');
    printf("Header Line: %s\n", header);
    
    // Parse version from header
    // Format: "Gamebryo File Format, Version x.x.x.x"
    int major, minor, patch, internal;
    if (sscanf(header, "Gamebryo File Format, Version %d.%d.%d.%d", &major, &minor, &patch, &internal) == 4) {
        printf("Parsed Version: %d.%d.%d.%d\n", major, minor, patch, internal);
    } else {
        printf("Could not parse version from header line\n");
    }
    
    // Read binary version (uint32)
    uint32_t binVersion;
    file.read((char*)&binVersion, 4);
    printf("\nBinary Version (at pos %lld): 0x%08X\n", (long long)file.tellg() - 4, binVersion);
    printf("  Decoded: %d.%d.%d.%d\n", 
           (binVersion >> 24) & 0xFF,
           (binVersion >> 16) & 0xFF, 
           (binVersion >> 8) & 0xFF,
           binVersion & 0xFF);
    
    // NIF 10.2.0.0 format (Gamebryo, user version 0):
    // After version (pos 39):
    //   - User Version (4 bytes) - should be 0
    //   - Num Blocks (4 bytes)
    //   - Num Block Types (2 bytes)
    //   - Block Type Strings (each: 4-byte len + string)
    //   - Block Type Index for each block (2 bytes each)
    //   - Block Sizes for each block (4 bytes each)
    //   - String Table
    //   - Block Data
    
    printf("\n=== Parsing NIF 10.2.0.0 Gamebryo format ===\n");
    
    uint32_t userVersion;
    file.read((char*)&userVersion, 4);
    printf("User Version: %u\n", userVersion);
    
    uint32_t numBlocks;
    file.read((char*)&numBlocks, 4);
    printf("Num Blocks: %u\n", numBlocks);
    
    uint16_t numBlockTypes;
    file.read((char*)&numBlockTypes, 2);
    printf("Num Block Types: %u\n", numBlockTypes);
    
    printf("\nBlock Types:\n");
    std::vector<std::string> blockTypeNames;
    for (uint16_t i = 0; i < numBlockTypes; i++) {
        uint32_t strLen;
        file.read((char*)&strLen, 4);
        char str[256] = {0};
        file.read(str, strLen);
        blockTypeNames.push_back(str);
        printf("  [%d] '%s'\n", i, str);
    }
    
    printf("\nBlock Type Indices (first 30):\n");
    std::vector<uint16_t> blockTypeIndices(numBlocks);
    for (uint32_t i = 0; i < numBlocks; i++) {
        file.read((char*)&blockTypeIndices[i], 2);
        if (i < 30) {
            printf("  Block[%d] = type %d (%s)\n", i, blockTypeIndices[i], 
                   blockTypeIndices[i] < blockTypeNames.size() ? blockTypeNames[blockTypeIndices[i]].c_str() : "INVALID");
        }
    }
    
    printf("\nBlock Sizes (first 30):\n");
    std::vector<uint32_t> blockSizes(numBlocks);
    for (uint32_t i = 0; i < numBlocks; i++) {
        file.read((char*)&blockSizes[i], 4);
        if (i < 30) {
            printf("  Block[%d] size = %u bytes\n", i, blockSizes[i]);
        }
    }
    
    // String table
    uint32_t numStrings;
    file.read((char*)&numStrings, 4);
    printf("\nNum Strings in table: %u\n", numStrings);
    
    uint32_t maxStringLen;
    file.read((char*)&maxStringLen, 4);
    printf("Max String Length: %u\n", maxStringLen);
    
    printf("\nStrings (first 50):\n");
    for (uint32_t i = 0; i < numStrings && i < 50; i++) {
        uint32_t strLen;
        file.read((char*)&strLen, 4);
        char str[512] = {0};
        if (strLen < 512) {
            file.read(str, strLen);
            printf("  [%d] '%s'\n", i, str);
        } else {
            printf("  [%d] len=%u TOO LONG\n", i, strLen);
            break;
        }
    }
    
    // Read some more to understand structure
    printf("\n=== Next 100 bytes as hex ===\n");
    uint8_t data[100];
    file.read((char*)data, 100);
    for (int i = 0; i < 100; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    
    printf("\n\n=== As string (printable only) ===\n");
    for (int i = 0; i < 100; i++) {
        if (data[i] >= 32 && data[i] < 127) printf("%c", data[i]);
        else printf(".");
    }
    printf("\n");
    
    return 0;
}

