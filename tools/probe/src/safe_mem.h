#pragma once
#include <cstddef>

namespace probe {

// copies size bytes from address into out guarded against access faults returns bytes copied or zero on fault
size_t safe_read(const void* address, void* out, size_t size);

}  // namespace probe
