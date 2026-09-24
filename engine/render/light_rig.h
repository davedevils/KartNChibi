#pragma once
#include "engine/render/day_night.h"
#include "engine/render/map_scene.h"

#include <string>

namespace KnC::Render {

// The lights of a KnC race read from World Light nif see docs reverse CLIENT SHADING
struct LightRig {
    // colour from NiAmbientLight
    HourColour ambient;
    // colour and travel direction from NiDirectionalLight
    SunLight   sun;
};

// False when the file cannot be read or holds no light error says why
bool load_light_rig(const std::string& path, LightRig& out, std::string& error);

// Puts the rig on a scene the ambient into every hour of its table
void apply_light_rig(const LightRig& rig, MapScene& scene);

}
