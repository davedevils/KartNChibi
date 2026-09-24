#include "safe_mem.h"
#include <cassert>
#include <cstdint>
#include <cstring>

int main() {
    uint8_t src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t dst[8] = {0};
    assert(probe::safe_read(src, dst, sizeof src) == sizeof src);
    assert(std::memcmp(src, dst, sizeof src) == 0);

    // reading from a null or invalid pointer must not crash and returns 0
    uint8_t scratch[4] = {0};
    assert(probe::safe_read(reinterpret_cast<const void*>(0), scratch, 4) == 0);
    assert(probe::safe_read(reinterpret_cast<const void*>(0xDEAD0000), scratch, 4) == 0);

    // a zero size read on a null address returns 0 without crashing
    assert(probe::safe_read(reinterpret_cast<const void*>(0), scratch, 0) == 0);
    return 0;
}
