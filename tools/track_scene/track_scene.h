#pragma once
// Loads one KnC track folder into a MapScene plus its collision and marker overlays

#include "engine/render/map_scene.h"
#include "games/kart/physics/client/gimmicks.h"
#include "games/kart/physics/client/world_collision.h"

#include <cstddef>
#include <string>
#include <vector>

namespace KnC::Tools {

// One track folder to load its map folder is the parent of track dir
struct TrackSceneRequest {
    std::string track_dir;          // Data Public World map track full path
    bool load_sun = true;
    bool load_geometry = false;     // geometry nif is a helper mesh drawing it hides the road
    bool load_collision = true;
    bool load_markers = true;       // build the start and gimmick csv marker boxes
    bool load_item_boxes = true;
    bool load_item_drums = true;    // the race of a speed mode builds no item manager so no drum
    bool point_textures_at_pak = true;  // open a pak reader for missing textures off when the caller set a source
};

// One question mark box of a race track the itembox ini rows place them
struct TrackItemBox {
    float position[3] = {0.f, 0.f, 0.f};
};

// One themed gimmick of the track world gimmick hit dispatch 0x4D3950 tests the car against these
struct TrackGimmick {
    float position[3] = {0.f, 0.f, 0.f};
    // the GimmickClass of gimmicks h read off the hit test the loader names
    int gimmick_class = -1;
};

// One loaded track the render scene plus the raw rows the physics side can reuse too
struct TrackScene {
    KnC::Render::MapScene scene;
    KnC::Kart::Client::ColTrack collision;
    std::vector<KnC::Kart::Client::GimmickStartRow> start_rows;
    // One box model per marker colour start first then the gimmick csv rows
    std::vector<KnC::Render::PropModel> marker_models;
    // World placements model index is local to marker models above
    std::vector<KnC::Render::PropInstance> marker_instances;
    // The question mark box model kept out of the scene the race view appends it once
    KnC::Render::PropModel item_box_model;
    bool has_item_box = false;
    // One spot per itembox ini row the race session hides one on a pickup
    std::vector<TrackItemBox> item_boxes;
    // One row per themed gimmick the loader placed the race screen sweeps them every tick
    std::vector<TrackGimmick> gimmicks;
};

// False only when the track folder or its track nif cannot be read error says why
bool load_track_scene(const TrackSceneRequest& request, TrackScene& out, std::string& error);

}
