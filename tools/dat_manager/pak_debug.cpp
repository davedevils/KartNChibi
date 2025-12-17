/**
 * Debug PAK structure around DDS files
 */
#include <stdio.h>
#include <stdint.h>

int main() {
    FILE* f = fopen("D:/KnC/KNC DEV/@git/DevClient/pak001.dat", "rb");
    if (!f) return 1;
    
    // Check what's before the first DDS at 0x1B882
    long ddsOffset = 0x1B882;
    
    printf("=== Data before DDS at 0x%lX ===\n", ddsOffset);
    
    // Read 300 bytes before
    fseek(f, ddsOffset - 300, SEEK_SET);
    uint8_t buffer[350];
    fread(buffer, 1, 350, f);
    
    printf("\nHex dump (last 100 bytes before DDS):\n");
    for (int i = 200; i < 300; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    
    printf("\nASCII (last 100 bytes before DDS):\n");
    for (int i = 200; i < 300; i++) {
        char c = (buffer[i] >= 32 && buffer[i] < 127) ? buffer[i] : '.';
        printf("%c", c);
    }
    printf("\n");
    
    // The DDS header
    printf("\nDDS header:\n");
    for (int i = 300; i < 350; i++) {
        printf("%02X ", buffer[i]);
        if ((i + 1 - 300) % 16 == 0) printf("\n");
    }
    
    // Now check for_sky_02.dds which we know exists
    printf("\n\n=== Searching for for_sky_02 ===\n");
    fseek(f, 0, SEEK_SET);
    
    uint8_t searchBuf[1024*1024];
    long pos = 0;
    while (fread(searchBuf, 1, sizeof(searchBuf), f) > 0) {
        for (int i = 0; i < sizeof(searchBuf) - 20; i++) {
            if (searchBuf[i] == 'f' && searchBuf[i+1] == 'o' && searchBuf[i+2] == 'r' &&
                searchBuf[i+3] == '_' && searchBuf[i+4] == 's' && searchBuf[i+5] == 'k' &&
                searchBuf[i+6] == 'y' && searchBuf[i+7] == '_' && searchBuf[i+8] == '0' &&
                searchBuf[i+9] == '2') {
                
                printf("Found at 0x%lX\n", pos + i);
                
                // Print context
                int start = i - 20;
                if (start < 0) start = 0;
                printf("Context: ");
                for (int j = start; j < i + 30 && j < sizeof(searchBuf); j++) {
                    char c = (searchBuf[j] >= 32 && searchBuf[j] < 127) ? searchBuf[j] : '.';
                    printf("%c", c);
                }
                printf("\n\n");
                
                // Find DDS after this
                for (int k = i; k < i + 500 && k < sizeof(searchBuf) - 4; k++) {
                    if (searchBuf[k] == 'D' && searchBuf[k+1] == 'D' && searchBuf[k+2] == 'S' && searchBuf[k+3] == ' ') {
                        printf("DDS found at 0x%lX (offset +%d from filename)\n", pos + k, k - i);
                        break;
                    }
                }
            }
        }
        pos += sizeof(searchBuf);
        if (pos > 50 * 1024 * 1024) break; // Stop after 50MB
    }
    
    fclose(f);
    return 0;
}

