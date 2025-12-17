// Simple PAK scanner to find path patterns
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE* f = fopen("D:/KnC/KNC DEV/@git/DevClient/pak001.dat", "rb");
    if (!f) { printf("Cannot open PAK\n"); return 1; }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("PAK size: %ld MB\n\n", size / 1024 / 1024);
    
    // Read header
    unsigned char header[256];
    fread(header, 1, 256, f);
    
    printf("Header:\n");
    for (int i = 0; i < 256; i += 16) {
        printf("%04X: ", i);
        for (int j = 0; j < 16; j++) printf("%02X ", header[i+j]);
        printf(" ");
        for (int j = 0; j < 16; j++) {
            char c = header[i+j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
    
    // Look for file table - often at end
    printf("\n=== Checking end of file ===\n");
    fseek(f, -4096, SEEK_END);
    unsigned char tail[4096];
    fread(tail, 1, 4096, f);
    
    // Look for "./" patterns
    for (int i = 0; i < 4000; i++) {
        if (tail[i] == '.' && tail[i+1] == '/') {
            int end = i;
            while (end < 4096 && tail[end] != 0 && end - i < 300) end++;
            if (end - i > 5 && end - i < 300) {
                printf("  End-%d: %.*s\n", 4096 - i, end - i, tail + i);
            }
        }
    }
    
    // Scan first 5MB for paths
    printf("\n=== Scanning for paths ===\n");
    fseek(f, 0, SEEK_SET);
    
    unsigned char* buf = (unsigned char*)malloc(5 * 1024 * 1024);
    size_t read = fread(buf, 1, 5 * 1024 * 1024, f);
    
    int found = 0;
    for (size_t i = 0; i < read - 100 && found < 30; i++) {
        // Look for "./Public" or "./Data" or similar
        if (buf[i] == '.' && buf[i+1] == '/' && 
            ((buf[i+2] == 'P' && buf[i+3] == 'u') ||  // Public
             (buf[i+2] == 'D' && buf[i+3] == 'a') ||  // Data
             (buf[i+2] == 'W' && buf[i+3] == 'o'))) { // World
            
            int end = i;
            while (end < read && buf[end] != 0 && buf[end] >= 32 && buf[end] < 127 && end - i < 300) end++;
            
            if (end - i > 10) {
                char path[301];
                int len = end - i;
                if (len > 300) len = 300;
                memcpy(path, buf + i, len);
                path[len] = 0;
                
                // Check if it ends with known extension
                if (strstr(path, ".nif") || strstr(path, ".dds") || 
                    strstr(path, ".wav") || strstr(path, ".xml") ||
                    strstr(path, ".png") || strstr(path, ".jpg")) {
                    printf("  0x%06lX: %s\n", (unsigned long)i, path);
                    
                    // Print bytes before path
                    printf("    Before: ");
                    for (int j = 16; j > 0; j--) {
                        if (i >= j) printf("%02X ", buf[i-j]);
                    }
                    printf("\n");
                    found++;
                }
            }
        }
    }
    
    free(buf);
    fclose(f);
    return 0;
}

