/// Col loader BSP walk surfaces ground height regen zones
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Kart::Client {

/// Shared half plane edge piece 0xc array stride 20 bytes world load col piece 0x4ECE90
struct ColEdge {
    int16_t childBehind = -1; ///< edge 0x00 0x02 next cell behind or front of plane -1 as 0xffff no cell
    int16_t childFront = -1;
    float a = 0.0f;           ///< edge 0x04 half plane A in equation A x plus B z plus C edge 0x08 half plane B
    float b = 0.0f;
    float c = 0.0f;           ///< edge 0x0c half plane C edge 0x10 two uint16 end vertex indices into piece 0x34 array low then high
    uint32_t reserved = 0;
};

/// One triangle cell piece 0x10 array stride 56 bytes world load col piece 0x4ECE90
struct ColCell {
    float heightA = 0.0f;      ///< cell 0x00 height plane A cell 0x04 height plane B
    float heightB = 0.0f;
    float heightC = 0.0f;      ///< cell 0x08 read but never multiplied by nonzero in traced path cell 0x0c height plane D
    float heightD = 0.0f;
    uint16_t edge[3] = { 0xffffu, 0xffffu, 0xffffu }; ///< cell 0x10 0x12 0x14 3 boundary edge indices cell 0x16 upper case ascii null terminated inside 34 bytes
    char surfaceName[34] = {};
};

static_assert(sizeof(ColEdge) == 20, "col edge record must match the file layout, 20 bytes");
static_assert(sizeof(ColCell) == 56, "col cell record must match the file layout, 56 bytes");

/// col piece 0x4ECE90 freed via world col piece free col point 12 bytes x y ground z height negated frame
struct ColPoint {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct ColPiece {
    std::vector<ColEdge> edges; ///< piece 0xc pointer 0x8 count 20 bytes each piece 0x10 pointer 0x1c count 56 bytes each
    std::vector<ColCell> cells;
    std::vector<ColPoint> zoneAnchors; ///< piece 0x18 pointer 0x14 count zone points unread piece 0x34 pointer 0x2c count mesh vertices for body get shape point
    std::vector<std::array<uint8_t, 12>> unreadD;
};

/// A track piece 0 is track col piece 1 to 8 are track1 col to track8 col
struct ColTrack {
    std::vector<ColPiece> pieces;
};

/// BSP walk context world bsp locate point 0x4EBFC0 wheel probe world probe wheel ground 0x486570
struct BspQuery {
    float x = 0.0f, z = 0.0f;          ///< ctx 0x04 0x08 current query point 0x0c current cell cursor
    const ColCell* cell = nullptr;
    float prevX = 0.0f, prevZ = 0.0f;   ///< ctx 0x10 0x14 point saved at start of locate call 0x18 cell saved at start of locate call
    const ColCell* prevCell = nullptr;
    mutable char surfaceName[35] = {};  ///< ctx 0x1c last upper case surface name cache behind query 0x3c piece this query is bound to
    const ColPiece* piece = nullptr;

    const ColPiece* pieceCache = nullptr; ///< wheel probe bookkeeping 0x486570 ctx 0x04 last piece pointer change detection 0x08 edge found by last locate that failed
    const ColEdge* lastEdge = nullptr;
    bool pieceIndexValid = false;         ///< wheel ctx 0x94 cached piece index still trusted 0x98 piece index parsed from surface name hash
    int cachedPieceIndex = -1;
    bool active = false;                  ///< wheel ctx 0x9c track loaded and probe armed
};

/// Car pose for world car find overlap 0x497F80 world car overlap test 0x497EE0 world car push apart 0x498800
struct CarPose {
    float x = 0.0f, y = 0.0f, z = 0.0f; ///< car 0x3244 0x3248 0x324c x y ground z height 0x3234 speed
    float speed = 0.0f;
    float yaw = 0.0f;                    ///< car 0x3220 yaw degrees not read by pair functions 0x2EA4 chassis extent y overlap radius source
    float extentY = 0.0f;
    float extentZ = 0.0f;                ///< car 0x2EA8 chassis extent z height overlap source 0x740 slot occupied
    uint8_t slotOccupied = 0;
    uint8_t finished = 0;                ///< car 0x9DA one skips the car in the overlap search
};

/// One regen ini row world load regen zones 0x48A910 four floats no reader in the exe
struct RegenZone {
    float x = 0.0f, y = 0.0f, z = 0.0f, radius = 0.0f; // world 0x1ADF810 plus 0x808 stride 0x10 count at 0x7FC never read
};

// loader world load col piece 0x4ECE90 world col piece free 0x4ECDB0 world load track pieces 0x485580
bool world_load_col_piece(ColPiece& piece, const std::string& filename, std::string& error);
void world_col_piece_free(ColPiece& piece);
bool world_load_track_pieces(ColTrack& track, const std::string& folder, std::string& error);

// bsp walk world bsp set piece 0x4EBCF0 world bsp edge sweep 0x4EBDD0 world bsp locate point 0x4EBFC0
bool world_bsp_set_piece(BspQuery& ctx, const ColPiece& piece, float x, float z);
int world_bsp_edge_sweep(BspQuery& ctx, float x, float z, float* outT);
bool world_bsp_locate_point(BspQuery& ctx, float x, float z, const ColCell** outCell = nullptr, float* outT = nullptr);
const char* world_query_surface_name(const BspQuery& ctx); // 0x4EC0B0

// height and ground world locate piece by height 0x4857A0 world ground test point 0x4858E0
bool world_locate_piece_by_height(BspQuery& ctx, const ColTrack& track, float x, float z, float yHint,
                                   float* outY, int* outSurfaceIndex);
bool world_ground_test_point(BspQuery& ctx, float x, float z);

// world place probe local 0x486300 spawn time reset bind piece 0 then the height scan arms the probe
bool world_place_probe_local(BspQuery& ctx, const ColTrack& track, float x, float z, float y, const ColPiece* piece,
                             bool worldReady);

// wheel probes 0x486570 0x4A0680 footprint is four points at car 0x3274 all must land in a cell
bool world_probe_wheel_ground(BspQuery& ctx, const ColTrack& track, const float footprintX[4],
                              const float footprintZ[4], int wheelIndex);
const char* world_wheel_query_surface(BspQuery& ctx, float wheelX, float wheelZ);
int world_wheel_surface_index(BspQuery& ctx, float wheelX, float wheelZ, bool trackActive); // 0x4A1310

// track flow world on track check 0x4A0750 world car push apart 0x498800 world stuck probe 0x498590
bool world_on_track_check(int progress);
// world car overlap test 0x497EE0 plane distance under 10 and under 0 8 times the two chassis y extents
bool world_car_overlap_test(const CarPose& a, const CarPose& b);
// world car find overlap 0x497F80 the first other occupied unfinished car overlapping in height and plane
int world_car_find_overlap(const CarPose* cars, int count, int selfIndex);
// world car push apart 0x498800 unit delta self to other nudge impact choice outEffectCar 0 self 1 other
bool world_car_push_apart(const CarPose& self, CarPose& other, float outDir[3], int* outEffectCar,
                          int* outEffectTier);
// world ground height at 0x485970 writes the cell plane height under x y into p 2
bool world_ground_height_at(BspQuery& ctx, float p[3]);
bool world_stuck_probe(BspQuery& ctx, float x, float z, float headingDeg);

void world_decode_surface_tag_bytes(char outTag[5]);  // tag bytes 0x487280 decodes call site to REGE tag digits 0x487450 decodes call site to LAVA
void world_decode_surface_tag_digits(char outTag[5]);
int world_wheel_surface_tag_count(BspQuery& ctx, const float wheelX[4], const float wheelZ[4],
                                   const char* tag); // 0x4A1380

// surfaces world surface name to index 0x4866D0 friction contact drag grip readers
int world_surface_name_to_index(const char* name);
float world_surface_friction(int index);     // friction 0x486D40 drag 0x486D70 was air penalty tick charges it per loaded wheel
float world_surface_contact_drag(int index);
float world_surface_grip(int index);        // grip 0x486DA0 throttle 0x5EB718 30 floats by standings row leaders lose a hair
float standings_rank_throttle_multiplier(int row);

// regen zones world load regen zones 0x48A910 up to 100 lines of 4 floats
bool world_load_regen_zones(const std::string& csvPath, std::vector<RegenZone>& zones, std::string& error);

} // namespace KnC Kart Client
