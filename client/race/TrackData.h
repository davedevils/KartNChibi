// resolves track id to World folders loads scene collision lines and boxes
#pragma once

#include "net/Catalog.h"

#include "games/kart/physics/client/gimmicks.h"
#include "tools/track_scene/track_scene.h"

#include <array>
#include <string>
#include <vector>

namespace KnC::Client {

// folders and catalogue values of one track
struct TrackFiles {
    int trackId = 0;
    std::string themeFolder;
    std::string trackFolder;
    std::string trackDir;
    std::string mapDir;
    uint32_t laps = 3;
    float tuning[3] = {0.4f, 0.6f, 90.f};
    std::string nameKey;
};

// one checkpoint point from COL head in same frame as start rows
struct CheckpointPoint3 {
    float x = 0.f, y = 0.f, z = 0.f;
};

// minimap for a track quad from minimap nif camera triple from minimap ini plus texture
struct MinimapData {
    bool valid = false;
    // four corners of quad in world x y in nif order with one uv each
    float cornerX[4] = {0.f, 0.f, 0.f, 0.f};
    float cornerY[4] = {0.f, 0.f, 0.f, 0.f};
    float cornerU[4] = {0.f, 0.f, 0.f, 0.f};
    float cornerV[4] = {0.f, 0.f, 0.f, 0.f};
    float quadZ = 0.f;
    // ini triple a b c stock camera sits at x b y minus a height c looking down
    float ini[3] = {0.f, 0.f, 0.f};
    // dds path relative to game folder as asset store reads it
    std::string texture;
};

// everything the race needs from disk for one track
struct RaceWorld {
    TrackFiles files;
    KnC::Tools::TrackScene scene;
    std::vector<KnC::Kart::Client::GimmickBoostRow> boostRows;
    std::vector<KnC::Kart::Client::GimmickItemboxRow> itemBoxes;
    // barrel rows from itemdrum ini itemdrum hit test sweeps them every tick
    std::vector<KnC::Kart::Client::GimmickItemdrumRow> itemDrums;
    // first dword of track COL START face is checkpoint zero CHECK NNN is NNN
    int checkpointCount = 0;
    std::vector<CheckpointPoint3> checkpointPoints;
    MinimapData minimap;
};

// 0x00C3 row and its 0x00C4 theme give World theme folder then track folder
bool resolveTrackFiles(const Catalog& catalog, int trackId, const std::string& gameDir, TrackFiles& out,
                       std::string& error);

// loads track scene boost and box rows and COL head
bool loadRaceWorld(const TrackFiles& files, RaceWorld& out, std::string& error);

// reads minimap nif and minimap ini of a track folder false when either is missing
bool loadMinimap(const std::string& gameDir, const TrackFiles& files, MinimapData& out);

// one entry under a folder found without case disk names differ from wire names
std::string findEntryCi(const std::string& dir, const std::string& name);
// Data Public Car Body High model BODY nif or empty
std::string kartBodyNif(const std::string& gameDir, const std::string& model);
// Data Car model car or empty
std::string kartCarFile(const std::string& gameDir, const std::string& model);
// Data Public Driver Body High asset body nif or empty
std::string driverBodyNif(const std::string& gameDir, const std::string& asset);

}
