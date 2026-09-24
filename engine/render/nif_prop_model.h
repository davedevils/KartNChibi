#pragma once
#include "engine/formats/nif_reader.h"
#include "engine/render/map_scene.h"

#include <string>

namespace KnC::Render {

// One NIF to turn into a PropModel and where its textures sit
struct NifModelRequest {
    std::string nif_path;
    // Folder the base texture names resolve in KnC ships textures beside the NIF
    std::string texture_dir;
    // Also play the controllers the stream ships stopped effects need it
    bool play_stopped_controllers = false;
};

// Bakes the node transforms onto the geometry one part per texture and state
void build_prop_model(const NifScene& scene, const NifModelRequest& request, PropModel& out);

// False only when the stream cannot be read error says why
bool load_prop_model(const NifModelRequest& request, PropModel& out, std::string& error);

}
