// Quick PAK header dump tool
#include <stdio.h>
#include <stdint.h>

int main() {
    FILE* f = fopen("D:/KnC/KNC DEV/@git/DevClient/pak001.dat", "rb");
    if (!f) { printf("Cannot open pak001.dat\n"); return 1; }
    
    uint8_t header[256];
    fread(header, 1, 256, f);
    
    printf("First 64 bytes (hex):\n");
    for (int i = 0; i < 64; i++) {
        printf("%02X ", header[i]);
        if ((i+1) % 16 == 0) printf("\n");
    }
    
    printf("\nFirst 64 bytes (ASCII):\n");
    for (int i = 0; i < 64; i++) {
        printf("%c", (header[i] >= 32 && header[i] < 127) ? header[i] : '.');
    }
    printf("\n");
    
    // Try reading as uint32_t
    uint32_t* u32 = (uint32_t*)header;
    printf("\nAs uint32:\n");
    for (int i = 0; i < 16; i++) {
        printf("[%2d] %10u (0x%08X)\n", i, u32[i], u32[i]);
    }
    
    fclose(f);
    return 0;
}

