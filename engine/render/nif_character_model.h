#pragma once
// Turns a skinned character NIF plus KF clips into the CharacterModel the renderer uploads
#include "engine/formats/nif_reader.h"
#include "engine/render/skinned_model.h"

#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Render {

// One KF stream to bind every sequence it holds becomes a clip
struct CharacterClipRequest {
    std::string kf_path;
    // Replaces the stream name when the file holds one sequence empty keeps it
    std::string name;
    // Animation id the game names the clip by minus one takes the clip position
    int32_t sequence_id = -1;
    // Position of the sequence inside the KF the KFM addresses minus one binds every one
    int32_t sequence_index = -1;
};

// One character NIF where its textures sit and the clips to bind on its skeleton
struct CharacterModelRequest {
    std::string nif_path;
    // Folder the base texture names resolve in same rule as the prop request
    std::string texture_dir;
    std::vector<CharacterClipRequest> clips;
};

// True when any geometry of the stream binds a NiSkinInstance
bool nif_has_skin(const NifScene& scene);

// Parts split by palette rig from the bone nodes clips bound by name stream already read
bool build_character_model(const NifScene& scene, const CharacterModelRequest& request,
                           CharacterModel& out, std::string& error);

// False when the NIF or one KF cannot be read error says which
bool load_character_model(const CharacterModelRequest& request, CharacterModel& out,
                          std::string& error);

}
