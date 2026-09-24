#include "engine/formats/nif_effect.h"

#include <iterator>

namespace KnC {

namespace {

// Wire order of NiAlphaProperty blend enum so a name and value are two views of one table
constexpr const char* kBlendNames[] = {
    "one", "zero", "src colour", "1-src colour", "dst colour", "1-dst colour",
    "src alpha", "1-src alpha", "dst alpha", "1-dst alpha", "src alpha saturate",
};

constexpr const char* kTestNames[] = {
    "always", "less", "equal", "less or equal",
    "greater", "not equal", "greater or equal", "never",
};

constexpr const char* kModifierNames[] = {
    "unknown", "age/death", "position", "bound update", "spawn", "grow/fade",
    "colour", "rotation", "gravity", "box emitter", "mesh emitter",
};

template <size_t Count>
bool find_name(const char* const (&names)[Count], const std::string& text, uint32_t& out) {
    for (size_t index = 0; index < Count; ++index) {
        if (text != names[index]) continue;
        out = static_cast<uint32_t>(index);
        return true;
    }
    return false;
}

} // namespace

const char* blend_function_name(NifBlendFunction function) {
    const size_t value = static_cast<size_t>(function);
    return value < std::size(kBlendNames) ? kBlendNames[value] : "out of range";
}

bool parse_blend_function(const std::string& text, NifBlendFunction& out) {
    uint32_t value = 0;
    if (!find_name(kBlendNames, text, value)) return false;
    out = static_cast<NifBlendFunction>(value);
    return true;
}

const char* test_function_name(NifTestFunction function) {
    const size_t value = static_cast<size_t>(function);
    return value < std::size(kTestNames) ? kTestNames[value] : "out of range";
}

NifShadedBy surface_shading(const NifSurfaceState& surface, bool has_base_texture) {
    // Replace hands the texture straight to the stage so client does not bother lighting vertex
    if (surface.texture_apply == NifTextureApplyMode::Replace && has_base_texture)
        return NifShadedBy::Unlit;
    const NifVertexMode vertex = surface.vertex_colour.vertex;
    if (surface.vertex_colour.lighting == NifLightingMode::EmissiveOnly) {
        // Emissive only asks for a vertex emissive the device cannot give it so client switches lighting off
        if (vertex == NifVertexMode::Emissive) return NifShadedBy::Unlit;
        // The other two leave every source on the material and the same pass disables lights
        return NifShadedBy::Material;
    }
    if (vertex == NifVertexMode::Emissive) return NifShadedBy::VertexEmissive;
    if (vertex == NifVertexMode::AmbientDiffuse) return NifShadedBy::VertexAmbientDiffuse;
    return NifShadedBy::Material;
}

const char* shaded_by_name(NifShadedBy shading) {
    switch (shading) {
        case NifShadedBy::Unlit:                return "unlit";
        case NifShadedBy::Material:             return "material";
        case NifShadedBy::VertexEmissive:       return "vertex emissive";
        case NifShadedBy::VertexAmbientDiffuse: return "vertex ambient+diffuse";
    }
    return "out of range";
}

const char* psys_modifier_kind_name(NifPSysModifierKind kind) {
    const size_t value = static_cast<size_t>(kind);
    return value < std::size(kModifierNames) ? kModifierNames[value] : "out of range";
}

} // namespace KnC
