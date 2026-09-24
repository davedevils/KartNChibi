#pragma once
// Write nif back copy block bytes source splice replacements
#include "engine/formats/nif_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// One block body replaced wholesale everything reader counted
struct NifBlockEdit {
    uint32_t    block = 0;
    std::string body;
};

// Added strings appended fixed string table order given
struct NifStreamEdit {
    std::vector<NifBlockEdit> blocks;
    std::vector<std::string>  added_strings;
};

// Whole stream rebuilt from source bytes parse checked
bool build_nif_stream(const std::string& source_bytes, const NifScene& scene,
                      const NifStreamEdit& edit, std::string& out, std::string& error);

// Same read source path write output path refused overwrite
bool write_nif_stream(const std::string& source_path, const NifScene& scene,
                      const NifStreamEdit& edit, const std::string& out_path,
                      std::string& error);

// Source bytes whole callers need them alongside parse
bool read_nif_bytes(const std::string& path, std::string& out);

} // namespace KnC
