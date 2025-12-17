// Analyze NKZIP format to understand file entry structure
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE* f = fopen("D:/KnC/KNC DEV/@git/DevClient/pak001.dat", "rb");
    if (!f) { printf("Cannot open\n"); return 1; }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("PAK size: %ld bytes\n\n", size);
    
    // Read first 10KB to analyze structure
    unsigned char* buf = (unsigned char*)malloc(50000);
    fread(buf, 1, 50000, f);
    
    printf("=== Header ===\n");
    for (int i = 0; i < 64; i += 16) {
        printf("%04X: ", i);
        for (int j = 0; j < 16; j++) printf("%02X ", buf[i+j]);
        printf(" ");
        for (int j = 0; j < 16; j++) {
            char c = buf[i+j];
            printf("%c", (c >= 32 && c < 127) ? c : '.');
        }
        printf("\n");
    }
    
    printf("\n=== First few entries ===\n");
    
    // The first entry starts at 0x20 based on earlier analysis
    // Let's analyze the structure more carefully
    
    int entryNum = 0;
    int pos = 0x20;
    
    while (pos < 10000 && entryNum < 5) {
        // Look for "./" pattern
        if (buf[pos] != '.' || buf[pos+1] != '/') {
            pos++;
            continue;
        }
        
        // Found path start
        int pathStart = pos;
        int pathEnd = pos;
        while (buf[pathEnd] != 0 && pathEnd - pathStart < 400) pathEnd++;
        
        char path[512];
        int pathLen = pathEnd - pathStart;
        memcpy(path, buf + pathStart, pathLen);
        path[pathLen] = 0;
        
        printf("\n--- Entry %d ---\n", entryNum);
        printf("Path at 0x%04X: %s (len=%d)\n", pathStart, path, pathLen);
        
        // Dump 20 bytes BEFORE path
        printf("20 bytes BEFORE path:\n  ");
        int beforeStart = pathStart - 20;
        if (beforeStart < 0) beforeStart = 0;
        for (int i = beforeStart; i < pathStart; i++) {
            printf("%02X ", buf[i]);
        }
        printf("\n");
        
        // Interpret as little-endian uint32s
        printf("  As uint32 LE (last 3): ");
        for (int i = pathStart - 12; i < pathStart; i += 4) {
            unsigned int val = *(unsigned int*)(buf + i);
            printf("0x%08X (%u)  ", val, val);
        }
        printf("\n");
        
        // Find end of null padding after path
        int dataStart = pathEnd + 1;
        // Skip any null padding - but check if it's truly padding or just zeros in path area
        
        // The path area seems to be fixed size, let's check
        printf("After null terminator (pathEnd+1 to +20):\n  ");
        for (int i = pathEnd + 1; i < pathEnd + 21 && i < 50000; i++) {
            printf("%02X ", buf[i]);
        }
        printf("\n");
        
        // Try to find next "./" to understand entry size
        int nextPath = pathEnd + 1;
        while (nextPath < 50000) {
            if (buf[nextPath] == '.' && buf[nextPath+1] == '/') break;
            nextPath++;
        }
        printf("Next entry at: 0x%04X (gap: %d bytes)\n", nextPath, nextPath - pathStart);
        
        // The gap between entries should tell us the structure
        // Let's look at what's in the gap
        int gapStart = pathEnd + 1;
        printf("Gap content (first 64 bytes after path null):\n");
        for (int row = 0; row < 4; row++) {
            printf("  %04X: ", gapStart + row*16);
            for (int col = 0; col < 16; col++) {
                printf("%02X ", buf[gapStart + row*16 + col]);
            }
            printf(" ");
            for (int col = 0; col < 16; col++) {
                char c = buf[gapStart + row*16 + col];
                printf("%c", (c >= 32 && c < 127) ? c : '.');
            }
            printf("\n");
        }
        
        entryNum++;
        pos = nextPath;
    }
    
    // Also analyze a .car file specifically
    printf("\n\n=== Looking for .car file ===\n");
    fseek(f, 0, SEEK_SET);
    unsigned char* bigbuf = (unsigned char*)malloc(200000);
    fread(bigbuf, 1, 200000, f);
    
    for (int i = 0x20; i < 195000; i++) {
        if (bigbuf[i] == '.' && bigbuf[i+1] == '/' && 
            strstr((char*)bigbuf+i, ".car") != NULL) {
            
            int pathEnd = i;
            while (bigbuf[pathEnd] != 0 && pathEnd - i < 400) pathEnd++;
            
            char carPath[512];
            memcpy(carPath, bigbuf + i, pathEnd - i);
            carPath[pathEnd - i] = 0;
            
            if (strstr(carPath, ".car") && strlen(carPath) < 100) {
                printf("Found .car at 0x%04X: %s\n", i, carPath);
                
                // Get size from before path
                unsigned int fileSize = *(unsigned int*)(bigbuf + i - 4);
                printf("Size hint: %u bytes (0x%X)\n", fileSize, fileSize);
                
                // Find data start (after path + padding)
                int dataStart = pathEnd + 1;
                while (bigbuf[dataStart] == 0 && dataStart - pathEnd < 400) dataStart++;
                
                printf("Data starts at: 0x%04X\n", dataStart);
                printf("First 128 bytes of .car data:\n");
                for (int row = 0; row < 8; row++) {
                    printf("  %04X: ", dataStart + row*16);
                    for (int col = 0; col < 16; col++) {
                        printf("%02X ", bigbuf[dataStart + row*16 + col]);
                    }
                    printf(" ");
                    for (int col = 0; col < 16; col++) {
                        char c = bigbuf[dataStart + row*16 + col];
                        printf("%c", (c >= 32 && c < 127) ? c : '.');
                    }
                    printf("\n");
                }
                break;
            }
        }
    }
    
    free(buf);
    free(bigbuf);
    fclose(f);
    return 0;
}

