/**
 * Search for specific files in pak001.dat
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char* argv[]) {
    const char* pakPath = "D:/KnC/KNC DEV/@git/DevClient/pak001.dat";
    const char* searchStr = argc > 1 ? argv[1] : "for_back";
    
    FILE* f = fopen(pakPath, "rb");
    if (!f) return 1;
    
    printf("Searching for '%s' in pak001.dat...\n\n", searchStr);
    
    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t buffer[65536];
    int found = 0;
    size_t searchLen = strlen(searchStr);
    
    for (long pos = 0; pos < fileSize; pos += 65536 - searchLen) {
        fseek(f, pos, SEEK_SET);
        size_t read = fread(buffer, 1, 65536, f);
        
        for (size_t i = 0; i < read - searchLen; i++) {
            bool match = true;
            for (size_t j = 0; j < searchLen && match; j++) {
                // Case insensitive
                char c1 = buffer[i + j];
                char c2 = searchStr[j];
                if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                if (c1 != c2) match = false;
            }
            
            if (match) {
                long matchPos = pos + i;
                printf("Found at 0x%08lX: ", matchPos);
                
                // Print context
                int start = (int)i - 20;
                if (start < 0) start = 0;
                int end = (int)i + 60;
                if (end > (int)read) end = (int)read;
                
                for (int j = start; j < end; j++) {
                    char c = (buffer[j] >= 32 && buffer[j] < 127) ? buffer[j] : '.';
                    printf("%c", c);
                }
                printf("\n");
                
                found++;
                if (found >= 30) {
                    printf("\n... (stopping at 30 matches)\n");
                    fclose(f);
                    return 0;
                }
            }
        }
    }
    
    printf("\nTotal found: %d\n", found);
    fclose(f);
    return 0;
}

