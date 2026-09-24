#include "safe_mem.h"
#include <cstring>
#include <windows.h>

namespace probe {

size_t safe_read(const void* address, void* out, size_t size) {
    if (size == 0) return 0;
    __try {
        memcpy(out, address, size);
        return size;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

}  // namespace probe
