// Diagnostic tool to understand NIF parsing issues
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>

struct NifHeader {
    std::string headerLine;
    uint32_t version;
    uint32_t userVersion;
    uint32_t numBlocks;
    std::vector<std::string> blockTypes;
    std::vector<uint16_t> blockTypeIndices;
};

bool ReadHeader(const std::vector<uint8_t>& data, NifHeader& hdr, size_t& pos) {
    // Header line
    while (pos < data.size() && data[pos] != '\n') {
        hdr.headerLine += (char)data[pos++];
    }
    pos++; // skip newline
    
    // Version
    hdr.version = *(uint32_t*)&data[pos]; pos += 4;
    
    // User version (for >= 10.0.1.8)
    if (hdr.version >= 0x0A000108) {
        hdr.userVersion = *(uint32_t*)&data[pos]; pos += 4;
    }
    
    // Num blocks
    hdr.numBlocks = *(uint32_t*)&data[pos]; pos += 4;
    
    // Block types
    uint16_t numTypes = *(uint16_t*)&data[pos]; pos += 2;
    for (uint16_t i = 0; i < numTypes; i++) {
        uint32_t len = *(uint32_t*)&data[pos]; pos += 4;
        std::string name((char*)&data[pos], len); pos += len;
        hdr.blockTypes.push_back(name);
    }
    
    // Block type indices
    hdr.blockTypeIndices.resize(hdr.numBlocks);
    for (uint32_t i = 0; i < hdr.numBlocks; i++) {
        hdr.blockTypeIndices[i] = *(uint16_t*)&data[pos]; pos += 2;
    }
    
    // Skip block sizes (only for >= 20.2.0.5)
    // Skip string table (only for >= 20.1.0.1)
    // Skip groups (only for >= 5.0.0.6)
    if (hdr.version >= 0x05000006) {
        uint32_t numGroups = *(uint32_t*)&data[pos]; pos += 4;
        pos += numGroups * 4; // skip group sizes
    }
    
    return true;
}

// Read a sized string (uint32 len + chars)
std::string ReadString(const std::vector<uint8_t>& data, size_t& pos) {
    uint32_t len = *(uint32_t*)&data[pos]; pos += 4;
    if (len == 0 || len > 0xFFFF) return "";
    std::string s((char*)&data[pos], len); pos += len;
    return s;
}

// Try to parse NiObjectNET fields and return bytes read
size_t ParseNiObjectNET(const std::vector<uint8_t>& data, size_t startPos) {
    size_t pos = startPos;
    
    // Name
    uint32_t nameLen = *(uint32_t*)&data[pos];
    printf("    name_len = %u (0x%08X)\n", nameLen, nameLen);
    
    if (nameLen > 0 && nameLen < 256) {
        pos += 4;
        std::string name((char*)&data[pos], nameLen);
        printf("    name = '%s'\n", name.c_str());
        pos += nameLen;
    } else if (nameLen == 0) {
        pos += 4;
        printf("    name = '' (empty)\n");
    } else {
        printf("    INVALID name_len!\n");
        return 0;
    }
    
    // numExtraData
    uint32_t numExtra = *(uint32_t*)&data[pos]; pos += 4;
    printf("    numExtraData = %u\n", numExtra);
    if (numExtra > 100) {
        printf("    INVALID numExtraData!\n");
        return 0;
    }
    pos += numExtra * 4; // skip extra data refs
    
    // controllerRef
    int32_t ctrlRef = *(int32_t*)&data[pos]; pos += 4;
    printf("    controllerRef = %d\n", ctrlRef);
    
    return pos - startPos;
}

int main(int argc, char* argv[]) {
    const char* path = argc > 1 ? argv[1] : 
        "D:/KnC/KNC DEV/@git/DevClient/Data/Public/Car/Body/High/Basic_1/BODY.nif";
    
    printf("=== NIF Diagnostic Tool ===\n");
    printf("File: %s\n\n", path);
    
    // Read file
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        printf("Failed to open file!\n");
        return 1;
    }
    size_t fileSize = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> data(fileSize);
    file.read((char*)data.data(), fileSize);
    file.close();
    
    printf("File size: %zu bytes\n\n", fileSize);
    
    // Parse header
    NifHeader hdr;
    size_t dataStart = 0;
    ReadHeader(data, hdr, dataStart);
    
    printf("Header: %s\n", hdr.headerLine.c_str());
    printf("Version: 0x%08X\n", hdr.version);
    printf("User Version: %u\n", hdr.userVersion);
    printf("Num Blocks: %u\n", hdr.numBlocks);
    printf("Data starts at: %zu\n\n", dataStart);
    
    // Parse blocks manually with detailed logging
    printf("=== Parsing blocks 14-20 in detail ===\n\n");
    
    size_t pos = dataStart;
    
    for (uint32_t i = 0; i < hdr.numBlocks && i < 25; i++) {
        std::string typeName = hdr.blockTypes[hdr.blockTypeIndices[i]];
        size_t blockStart = pos;
        
        if (i >= 14 && i <= 20) {
            printf("Block[%u] %s at offset %zu:\n", i, typeName.c_str(), blockStart);
            
            // Show raw bytes
            printf("  Raw bytes: ");
            for (int j = 0; j < 20 && blockStart + j < fileSize; j++) {
                printf("%02X ", data[blockStart + j]);
            }
            printf("...\n");
            
            // Try to parse NiObjectNET header
            if (typeName.find("Ni") == 0 && typeName != "NiTriStripsData") {
                printf("  Attempting NiObjectNET parse:\n");
                size_t objNetBytes = ParseNiObjectNET(data, blockStart);
                if (objNetBytes > 0) {
                    printf("  NiObjectNET: %zu bytes\n", objNetBytes);
                }
            }
            printf("\n");
        }
        
        // Skip to next block (we don't know exact sizes without parsing)
        // This is just for diagnostic - we'll stop after block 20
        if (i >= 20) break;
        
        // Very rough size estimation - just for getting to interesting blocks
        if (i < 14) {
            // Trust previous parsing up to block 14
            if (typeName == "NiNode") pos += 100 + (i == 0 ? 130 : 0);
            else if (typeName == "NiZBufferProperty") pos += 18;
            else if (typeName == "NiVertexColorProperty") pos += 22;
            else if (typeName == "NiTransformController") pos += 30;
            else if (typeName == "NiTransformInterpolator") pos += 36;
            else if (typeName == "NiTransformData") pos += 290;
            else if (typeName == "NiTriStrips") pos += 120;
            else if (typeName == "NiStencilProperty") pos += 41;
            else if (typeName == "NiMaterialProperty") pos += 74;
            else if (typeName == "NiSpecularProperty") pos += 14;
            else if (typeName == "NiAlphaProperty") pos += 15;
            else if (typeName == "NiTexturingProperty") pos += 52;
            else if (typeName == "NiSourceTexture") pos += 48;
            else if (typeName == "NiTriStripsData") pos += 29420;
            else pos += 50; // guess
        }
    }
    
    // Find O_WHEEL strings directly
    printf("\n=== Searching for O_WHEEL strings ===\n");
    for (size_t i = 0; i < fileSize - 7; i++) {
        if (memcmp(&data[i], "O_WHEEL", 7) == 0) {
            printf("Found O_WHEEL at offset %zu\n", i);
            // Show context
            printf("  Context (-10 to +20): ");
            size_t start = (i >= 10) ? i - 10 : 0;
            for (size_t j = start; j < i + 20 && j < fileSize; j++) {
                printf("%02X ", data[j]);
            }
            printf("\n");
        }
    }
    
    return 0;
}

