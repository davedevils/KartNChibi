// Analyze NKZIP - find DDS and NIF to understand large file storage
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    FILE* f = fopen("D:/KnC/KNC DEV/@git/DevClient/pak001.dat", "rb");
    if (!f) { printf("Cannot open\n"); return 1; }
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    printf("PAK size: %ld bytes (%.2f MB)\n\n", fileSize, fileSize / 1024.0 / 1024.0);
    
    // Read larger chunk
    unsigned char* buf = (unsigned char*)malloc(500000);
    fread(buf, 1, 500000, f);
    
    // Find first .dds entry
    printf("=== Looking for first .dds entry ===\n");
    for (int i = 0x20; i < 490000; i++) {
        if (buf[i] == '.' && buf[i+1] == '/' && buf[i+2] == 'D') {
            int pathEnd = i;
            while (buf[pathEnd] != 0 && pathEnd - i < 400) pathEnd++;
            
            char path[512];
            memcpy(path, buf + i, pathEnd - i);
            path[pathEnd - i] = 0;
            
            if (strstr(path, ".dds") && strlen(path) < 100) {
                printf("Found .dds entry at 0x%05X: %s\n", i, path);
                
                // Size is 4 bytes before path
                unsigned int size = *(unsigned int*)(buf + i - 4);
                printf("Size: %u bytes (0x%X)\n", size, size);
                
                // Look at bytes before path (entry header)
                printf("12 bytes before path:\n  ");
                for (int j = i - 12; j < i; j++) {
                    printf("%02X ", buf[j]);
                }
                printf("\n  As values: ");
                printf("u32[0]=0x%08X  ", *(unsigned int*)(buf + i - 12));
                printf("u32[1]=0x%08X  ", *(unsigned int*)(buf + i - 8));
                printf("u32[2]=0x%08X (size=%u)\n", *(unsigned int*)(buf + i - 4), *(unsigned int*)(buf + i - 4));
                
                // Find where the data actually is
                // For small files, data is inline after the path
                // For large files, maybe there's an offset?
                
                // Calculate entry boundaries
                int entryStart = i - 12;
                
                // The path area seems to be 256 bytes
                int pathAreaStart = i;
                int dataStart = pathAreaStart + 256;  // After 256-byte path area
                
                printf("Entry starts at: 0x%05X\n", entryStart);
                printf("Calculated data start: 0x%05X\n", dataStart);
                
                // Check if DDS magic is at dataStart
                printf("At dataStart: %02X %02X %02X %02X (looking for 'DDS ')\n",
                       buf[dataStart], buf[dataStart+1], buf[dataStart+2], buf[dataStart+3]);
                
                // Also search for DDS magic nearby
                printf("Searching for 'DDS ' magic nearby...\n");
                for (int search = i; search < i + 1000 && search < 490000; search++) {
                    if (buf[search] == 'D' && buf[search+1] == 'D' && 
                        buf[search+2] == 'S' && buf[search+3] == ' ') {
                        printf("  Found DDS magic at 0x%05X (offset from path: +%d)\n", 
                               search, search - i);
                        
                        // Show DDS header
                        printf("  DDS header:\n    ");
                        for (int k = 0; k < 32; k++) {
                            printf("%02X ", buf[search + k]);
                        }
                        printf("\n");
                        break;
                    }
                }
                
                break;
            }
        }
    }
    
    // Now find first .nif
    printf("\n=== Looking for first .nif entry ===\n");
    for (int i = 0x20; i < 490000; i++) {
        if (buf[i] == '.' && buf[i+1] == '/' && buf[i+2] == 'D') {
            int pathEnd = i;
            while (buf[pathEnd] != 0 && pathEnd - i < 400) pathEnd++;
            
            char path[512];
            memcpy(path, buf + i, pathEnd - i);
            path[pathEnd - i] = 0;
            
            if (strstr(path, ".nif") && strlen(path) < 100) {
                printf("Found .nif entry at 0x%05X: %s\n", i, path);
                
                unsigned int size = *(unsigned int*)(buf + i - 4);
                printf("Size: %u bytes (0x%X)\n", size, size);
                
                // Search for NIF magic "Gamebryo" or "NetImmerse"
                printf("Searching for NIF magic nearby...\n");
                for (int search = i; search < i + 5000 && search < 490000; search++) {
                    if ((buf[search] == 'G' && buf[search+1] == 'a' && buf[search+2] == 'm' && buf[search+3] == 'e') ||
                        (buf[search] == 'N' && buf[search+1] == 'e' && buf[search+2] == 't' && buf[search+3] == 'I')) {
                        printf("  Found NIF magic at 0x%05X: %.30s\n", search, buf + search);
                        break;
                    }
                }
                break;
            }
        }
    }
    
    // Let's also check: what is the ACTUAL entry structure?
    printf("\n=== Entry structure analysis ===\n");
    
    // First entry at 0x20
    printf("Entry 0 header (at 0x20):\n");
    for (int i = 0x20; i < 0x20 + 16; i += 4) {
        printf("  0x%02X: 0x%08X (%u)\n", i, *(unsigned int*)(buf + i), *(unsigned int*)(buf + i));
    }
    
    // What's at 0x20?
    // 82 2A DF 35 = CRC32?
    // 34 52 00 00 = offset or something?
    // A0 01 00 00 = size (416)
    
    // Let's see if 0x5234 is an offset
    printf("\nChecking if 0x5234 is an offset to data...\n");
    printf("At offset 0x5234: %02X %02X %02X %02X\n", 
           buf[0x5234], buf[0x5234+1], buf[0x5234+2], buf[0x5234+3]);
    
    // Hmm, let's try reading from much further in the file for actual data
    printf("\n=== Searching for DDS files in entire PAK ===\n");
    
    // Seek to middle of file and look for DDS magic
    fseek(f, fileSize / 2, SEEK_SET);
    unsigned char* midbuf = (unsigned char*)malloc(100000);
    fread(midbuf, 1, 100000, f);
    
    int ddsCount = 0;
    for (int i = 0; i < 95000 && ddsCount < 3; i++) {
        if (midbuf[i] == 'D' && midbuf[i+1] == 'D' && 
            midbuf[i+2] == 'S' && midbuf[i+3] == ' ') {
            printf("Found DDS at offset 0x%lX + 0x%X\n", fileSize/2, i);
            ddsCount++;
        }
    }
    
    free(buf);
    free(midbuf);
    fclose(f);
    return 0;
}

