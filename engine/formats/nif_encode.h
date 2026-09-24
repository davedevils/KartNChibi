#pragma once
// Encode nif block from parsed fields not source bytes complete decoder families
#include "engine/formats/nif_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC {

// Check if encode nif block can write this block type one of three
bool can_encode_nif_block(const NifBlock& block);

// Emit block body from decoded fields false names refusal type or field
bool encode_nif_block(const NifHeader& header, const NifBlock& block, std::string& out,
                      std::string& refusal);

// Bare geometry never in stream arrays from DCC tool positions x y z
struct NifTriShapeGeometry {
    std::vector<float>    positions;
    std::vector<float>    normals;
    std::vector<float>    uvs;
    std::vector<float>    colours;
    std::vector<uint16_t> triangles;   // three corner indices a face
};

// Build NiTriShapeData from geometry compute bounding sphere 65535 vertices max
bool build_nif_tri_shape_data(const NifTriShapeGeometry& geometry, NifBlock& out,
                              std::string& refusal);

} // namespace KnC
