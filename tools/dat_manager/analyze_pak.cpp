// Quick PAK analyzer to understand NKZIP structure
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>

int main() {
    const char* pakPath = "D:/KnC/KNC DEV/@git/DevClient/pak001.dat";
    
    FILE* f = fopen(pakPath, "rb");
    if (!f) {
        printf("Cannot open %s\n", pakPath);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("=== NKZIP Analysis ===\n");
    printf("File size: %ld bytes (%.2f MB)\n\n", fileSize, fileSize / 1024.0 / 1024.0);
    
    // Read header
    uint8_t header[64];
    fread(header, 1, 64, f);
    
    printf("Header hex dump:\n");
    for (int i = 0; i < 64; i += 16) {
        printf("%04X: ", i);
        for (int j = 0; j < 16; j++) printf("%02X ", header[i+j]);
        printf(" ");
        for (int j = 0; j < 16; j++) {
            char c = header[i+j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
    
    printf("\nMagic: %.8s\n", header);
    
    // Parse header values
    uint32_t* h32 = (uint32_t*)header;
    printf("Header values:\n");
    for (int i = 0; i < 16; i++) {
        printf("  [%02d] 0x%02X: %10u (0x%08X)\n", i, i*4, h32[i], h32[i]);
    }
    
    // The structure appears to be:
    // After header, there's a file table with entries like:
    // [path_len][path][offset][size] or similar
    
    printf("\n=== Scanning for file paths ===\n");
    
    // Read first 100KB to find path patterns
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> buf(100 * 1024);
    fread(buf.data(), 1, buf.size(), f);
    
    int pathCount = 0;
    for (size_t i = 0; i < buf.size() - 100 && pathCount < 50; i++) {
        // Look for "./Public" or "./Data" patterns
        if (buf[i] == '.' && buf[i+1] == '/') {
            // Check if it looks like a path
            bool valid = true;
            int end = i;
            while (end < (int)buf.size() && buf[end] != 0 && end - i < 300) {
                char c = buf[end];
                if (c < 32 || c > 126) { valid = false; break; }
                end++;
            }
            
            if (valid && end - i > 5 && end - i < 300) {
                std::string path((char*)&buf[i], end - i);
                
                // Check for common extensions
                if (path.find(".nif") != std::string::npos ||
                    path.find(".dds") != std::string::npos ||
                    path.find(".wav") != std::string::npos ||
                    path.find(".ogg") != std::string::npos ||
                    path.find(".jpg") != std::string::npos ||
                    path.find(".png") != std::string::npos ||
                    path.find(".xml") != std::string::npos ||
                    path.find(".txt") != std::string::npos ||
                    path.find(".ini") != std::string::npos) {
                    
                    // Print 8 bytes before and values
                    printf("\nPath at 0x%06X: %s\n", (unsigned)i, path.c_str());
                    printf("  Before: ");
                    for (int j = 16; j > 0; j--) {
                        if (i >= j) printf("%02X ", buf[i-j]);
                    }
                    printf("\n");
                    
                    // Check values before path
                    if (i >= 8) {
                        uint32_t v1 = *(uint32_t*)&buf[i-8];
                        uint32_t v2 = *(uint32_t*)&buf[i-4];
                        printf("  Val[-8]: %u, Val[-4]: %u\n", v1, v2);
                    }
                    
                    pathCount++;
                    i = end;
                }
            }
        }
    }
    
    printf("\n=== File table search ===\n");
    
    // Try to find the file table - often at the end or start
    // Search for concentration of paths
    
    fclose(f);
    return 0;
}

