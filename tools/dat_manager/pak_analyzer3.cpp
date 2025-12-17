/**
 * NKZIP format - try to understand the structure better
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main() {
    const char* pakPath = "D:/KnC/KNC DEV/@git/DevClient/pak001.dat";
    
    FILE* f = fopen(pakPath, "rb");
    if (!f) return 1;
    
    printf("=== NKZIP Structure Analysis ===\n\n");
    
    // Read more of the header
    uint8_t header[1024];
    fread(header, 1, 1024, f);
    
    // Print as hex with annotations
    printf("Offset  | Hex                                              | ASCII\n");
    printf("--------|--------------------------------------------------|----------------\n");
    
    for (int row = 0; row < 64; row++) {
        printf("0x%04X  | ", row * 16);
        for (int col = 0; col < 16; col++) {
            printf("%02X ", header[row * 16 + col]);
        }
        printf("| ");
        for (int col = 0; col < 16; col++) {
            char c = header[row * 16 + col];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
    
    // The structure seems to be:
    // 0x00: "NKZIP\0\0\0" (8 bytes magic)
    // 0x08: Some flags or version
    // 0x10: Number "1" or version
    // 0x20: First entry starts here
    //       - 4 bytes: compressed size (or total size?)
    //       - 4 bytes: uncompressed size or offset  
    //       - 4 bytes: path length
    //       - N bytes: path string
    
    printf("\n=== Trying different offset interpretations ===\n");
    
    // At offset 0x20 (32):
    uint32_t* data32 = (uint32_t*)(header + 0x20);
    printf("At 0x20: %u (0x%X)\n", data32[0], data32[0]);
    printf("At 0x24: %u (0x%X)\n", data32[1], data32[1]);
    printf("At 0x28: %u (0x%X) = path length?\n", data32[2], data32[2]);
    
    // Read path at 0x2C
    printf("At 0x2C: '%s'\n", header + 0x2C);
    
    // After the first path, what's next?
    int pathLen = strlen((char*)(header + 0x2C));
    int nextOffset = 0x2C + pathLen + 1;
    printf("\nNext entry should be at 0x%X\n", nextOffset);
    
    // Check if there's a file table at the end of file
    printf("\n=== Checking end of file ===\n");
    fseek(f, -1024, SEEK_END);
    long endPos = ftell(f);
    printf("Reading from position: %ld\n", endPos);
    
    uint8_t tail[1024];
    fread(tail, 1, 1024, f);
    
    // Look for paths in the tail
    for (int i = 0; i < 1024 - 10; i++) {
        if (tail[i] == '.' && tail[i+1] == '/' && tail[i+2] == 'D') {
            printf("Found path at end-%d: %.60s\n", 1024-i, tail + i);
        }
    }
    
    // The file might be:
    // [Header][Compressed data block][File table at end]
    
    printf("\n=== Looking for file count ===\n");
    // Check various positions for a file count
    fseek(f, 8, SEEK_SET);
    uint32_t val;
    fread(&val, 4, 1, f);
    printf("At 0x08: %u\n", val);
    
    fseek(f, 16, SEEK_SET);
    fread(&val, 4, 1, f);
    printf("At 0x10: %u (could be version or file count)\n", val);
    
    // Let's check if offset 0x5234 points to data
    printf("\n=== Data at offset 0x5234 ===\n");
    fseek(f, 0x5234, SEEK_SET);
    uint8_t dataStart[64];
    fread(dataStart, 1, 64, f);
    
    // Check for zlib header (78 9C or 78 DA)
    printf("First bytes: %02X %02X %02X %02X\n", 
           dataStart[0], dataStart[1], dataStart[2], dataStart[3]);
    
    if (dataStart[0] == 0x78 && (dataStart[1] == 0x9C || dataStart[1] == 0xDA)) {
        printf("ZLIB compressed data detected!\n");
    }
    
    // Check for NIF header
    if (memcmp(dataStart, "Gamebryo", 8) == 0 || 
        memcmp(dataStart, "NetImmerse", 10) == 0) {
        printf("NIF file header detected!\n");
    }
    
    fclose(f);
    return 0;
}

