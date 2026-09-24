#pragma once
// Changing a value in nif without re serialising the block that holds it reader is lossy by design
#include "engine/formats/nif_reader.h"

#include <string>
#include <vector>

namespace KnC {

// The block own bytes exactly the span build nif stream would have copied
std::string nif_block_body(const std::string& source_bytes, const NifBlock& block);

// The transform the source states for this block decoded out of body at the offset reader recorded
bool read_nif_transform(const NifBlock& block, const std::string& body, NifTransform& out);

// Writes thirteen floats of NiAVObject transform over the bytes the decoder read them from
bool splice_nif_transform(const NifBlock& block, const NifTransform& transform,
                          std::string& body);

// The NiMaterialProperty the source states for this block decoded out of body at offset recorded
bool read_nif_material(const NifBlock& block, const std::string& body, NifMaterialState& out);

// Writes fourteen floats of NiMaterialProperty over the bytes the decoder read them from
bool splice_nif_material(const NifBlock& block, const NifMaterialState& material,
                         std::string& body);

// Block name from 20 1 0 1 an index into the header string table else a counted string
bool read_nif_name(const NifHeader& header, const NifBlock& block, const std::string& body,
                   std::string& out);

// Do this splice last below 20 1 0 1 a longer name moves every field behind it
bool splice_nif_name(const NifHeader& header, const NifBlock& block, const std::string& name,
                     std::string& body, std::vector<std::string>& added_strings);

// One value of one property block named so a caller does not have to know how stream wrote it
enum class NifPropertyValue {
    AlphaFlags,
    AlphaThreshold,
    DepthFlags,
    LightingMode,      // NiVertexColorProperty
    VertexMode,
    // NiTexturingProperty
    TextureApplyMode,
    TextureClampMode,
};

// Writes one back over the bytes the decoder read it from fixed width so body keeps its length
bool read_nif_property_value(const NifBlock& block, NifPropertyValue which,
                             const std::string& body, uint32_t& out);

// Which of geometry data block vertex arrays a splice names
bool splice_nif_property_value(const NifBlock& block, NifPropertyValue which, uint32_t value,
                               std::string& body);

const char* nif_property_value_name(NifPropertyValue which);

// Writes one vertex array back over the bytes the decoder read it from a splice body keeps its length
enum class NifVertexArray { Positions, Normals, TextureCoordinates, Colours };

bool splice_nif_vertex_array(const NifBlock& block, NifVertexArray which,
                             const std::vector<float>& values, std::string& body,
                             std::string& refusal);

} // namespace KnC
