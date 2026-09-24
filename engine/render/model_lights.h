#pragma once
// The lights and the sphere environment maps a nif hangs over its own tree through node effect lists
#include "engine/formats/nif_reader.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Render {

// Directional lights of the model the device adds beside the sun the MAX exporter bakes two
constexpr int kModelLights = 2;

// NiDirectionalLight and NiAmbientLight of the root effect lists directions travel in model space
struct ModelLights {
    int   count = 0;
    float direction[kModelLights][3] = {};
    // Diffuse times dimmer
    float colour[kModelLights][3] = {};
    // Ambient times dimmer of every NiAmbientLight summed the global ambient takes it
    float ambient[3] = {0.f, 0.f, 0.f};
};

// A NiTextureEffect sphere map the file and the model space rotation of the effect node
struct EnvironmentMap {
    std::string texture;
    float rotation[9] = {1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f};
};

// The lights the effect lists of the scene roots carry point lights and spot lights stay out
ModelLights collect_model_lights(const NifScene& scene);

// The sphere map of the nearest effect list above each block the way NiDynamicEffectState gathers it
class EnvironmentMaps {
public:
    explicit EnvironmentMaps(const NifScene& scene);
    // The map over the block an empty texture when none reaches it
    const EnvironmentMap& of(uint32_t block) const;

private:
    std::vector<int> map_of_block_;
    std::vector<EnvironmentMap> maps_;
};

}
