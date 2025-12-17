/**
 * PAK file format analyzer
 * Reads pak001.dat header to understand the archive structure
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main() {
    const char* pakPath = "D:/KnC/KNC DEV/@git/DevClient/pak001.dat";
    
    FILE* f = fopen(pakPath, "rb");
    if (!f) {
        printf("Cannot open %s\n", pakPath);
        return 1;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("=== PAK Analyzer ===\n");
    printf("File: %s\n", pakPath);
    printf("Size: %ld bytes (%.2f MB)\n\n", fileSize, fileSize / 1024.0 / 1024.0);
    
    // Read first 256 bytes
    uint8_t header[256];
    fread(header, 1, 256, f);
    
    printf("=== First 64 bytes (hex) ===\n");
    for (int i = 0; i < 64; i++) {
        printf("%02X ", header[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    
    printf("\n=== First 64 bytes (ASCII) ===\n");
    for (int i = 0; i < 64; i++) {
        char c = (header[i] >= 32 && header[i] < 127) ? header[i] : '.';
        printf("%c", c);
        if ((i + 1) % 64 == 0) printf("\n");
    }
    
    printf("\n\n=== As uint32 ===\n");
    uint32_t* u32 = (uint32_t*)header;
    for (int i = 0; i < 16; i++) {
        printf("[%2d] %10u (0x%08X)\n", i*4, u32[i], u32[i]);
    }
    
    // Common PAK formats have:
    // - Magic number at start
    // - File count
    // - File table offset or inline table
    
    printf("\n=== Guessing structure ===\n");
    
    // Check if it starts with a known magic
    char magic[5] = {0};
    memcpy(magic, header, 4);
    printf("Magic (4 bytes): '%s' (0x%08X)\n", magic, u32[0]);
    
    // Try to find file entries by looking for .nif, .dds patterns
    printf("\n=== Searching for file patterns ===\n");
    
    // Read more data to find patterns
    uint8_t buffer[4096];
    fseek(f, 0, SEEK_SET);
    
    // Search for .nif or .dds in first 1MB
    int found = 0;
    for (long pos = 0; pos < 1024*1024 && pos < fileSize; pos += 4096) {
        fseek(f, pos, SEEK_SET);
        size_t read = fread(buffer, 1, 4096, f);
        
        for (size_t i = 0; i < read - 4; i++) {
            if ((buffer[i] == '.' && buffer[i+1] == 'n' && buffer[i+2] == 'i' && buffer[i+3] == 'f') ||
                (buffer[i] == '.' && buffer[i+1] == 'N' && buffer[i+2] == 'I' && buffer[i+3] == 'F') ||
                (buffer[i] == '.' && buffer[i+1] == 'd' && buffer[i+2] == 'd' && buffer[i+3] == 's') ||
                (buffer[i] == '.' && buffer[i+1] == 'D' && buffer[i+2] == 'D' && buffer[i+3] == 'S')) {
                
                // Print context around the match
                long matchPos = pos + i;
                printf("Found at 0x%08lX: ", matchPos);
                
                // Go back to find start of filename
                int start = (int)i - 50;
                if (start < 0) start = 0;
                
                for (int j = start; j < (int)i + 10 && j < (int)read; j++) {
                    char c = (buffer[j] >= 32 && buffer[j] < 127) ? buffer[j] : '.';
                    printf("%c", c);
                }
                printf("\n");
                
                found++;
                if (found >= 20) break;
            }
        }
        if (found >= 20) break;
    }
    
    printf("\nFound %d file references in first 1MB\n", found);
    
    fclose(f);
    return 0;
}

