#include "world_collision.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace KnC::Kart::Client {

namespace {

constexpr float kEpsilon = 0.0f;
constexpr float kSweepBound = 1.0f;
constexpr float kHeightTolerance = 5.0f;
constexpr int kBspStepCap = 0x3ff;        // 1024 step safety cap world bsp locate point 0x4EBFC0

constexpr float kDefaultFriction = 1.0f;    // world surface friction 0x486D40 grip 0x486DA0 out of range default 0x59F480 grip default 0x59F44C
constexpr float kDefaultGripOrPenalty = 0.0f;

constexpr float kPushApartNudge = 0.06f;      // world car push apart 0x498800 velocity scale 0x3F7E76C9 in constants h as kPushApartVelocityScale nudge 0x5A6A44 high speed 0x5A3244
constexpr float kPushApartHighSpeed = 30.0f;
constexpr float kPushApartLowSpeed = 15.0f;   // low speed 0x5A3230 dir scale 0x5A322C
constexpr float kPushApartDirScale = 0.8f;
constexpr float kOverlapMaxDistance = 10.0f;  // overlap max distance 0x59F404 extent scale 0x5A322C
constexpr float kOverlapExtentScale = 0.8f;

constexpr float kStuckOuterCenter = 90.0f; // world stuck probe 0x498590 point builder FUN 0x0044DEC0 outer center 0x5A323C outer step 0x5A3238
constexpr float kStuckOuterStep = 5.0f;
constexpr float kStuckOuterBound = 50.0f;  // outer bound 0x59F450 inner step 0x5A15EC
constexpr float kStuckInnerStep = 0.2f;
constexpr float kStuckInnerBound = 2.0f;   // inner bound 0x5A24EC deg to rad 0x5A1E88
constexpr float kDegToRad = 0.0174533f;

// world decode surface tag bytes 0x487280 decodes to REGE
constexpr uint8_t kRegeSourceBytes[4] = { 0x53, 0x47, 0x4a, 0x49 };
// world decode surface tag digits 0x487450 decodes to LAVA
constexpr int32_t kLavaSourceDigits[4] = { 0xb21, 0x9af, 0xcdd, 0x9f9 };

} // namespace

// 9 row surface table 0x5EA880 friction contact drag grip category stride 16
struct SurfaceRow {
    float friction;
    float contactDrag;
    float grip;
    int32_t category;
};

constexpr SurfaceRow kSurfaceTable[9] = { // 0x5EA880
    { 1.0f, 0.0f, 1.0f, 0 },
    { 1.0f, 0.0f, 0.2f, 0 },     // 1 ASPHALT 2 WATER
    { 1.0f, 0.01f, 0.2f, 2 },
    { 0.9f, 0.0f, 0.4f, 1 },     // 3 GRASS 4 SNOW
    { 0.6f, 0.0f, 0.5f, 3 },
    { 0.4f, -0.001f, 0.3f, 3 },  // 5 ICE 6 BOARD
    { 1.0f, 0.0f, 0.2f, 0 },
    { 0.8f, 0.02f, 0.2f, 2 },
    { 0.9f, 0.01f, 0.4f, 1 },
};

// 30 float table 0x5EB718 by standings row the throttle factor of the first seven rows sits under 1
constexpr float kRankThrottleTable[30] = {
    0.99934f, 0.99941f, 0.99947f, 0.99954f, 0.99960f, 0.99967f, 0.99983f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
};

// loader ---------------------------------------------------------------

void world_col_piece_free(ColPiece& piece) {
    // world col piece free 0x4ECDB0 RAII clears 4 arrays
    piece.cells.clear();
    piece.cells.shrink_to_fit();
    piece.edges.clear();
    piece.edges.shrink_to_fit();
    piece.unreadD.clear();
    piece.unreadD.shrink_to_fit();
    piece.zoneAnchors.clear();
    piece.zoneAnchors.shrink_to_fit();
}

bool world_load_col_piece(ColPiece& piece, const std::string& filename, std::string& error) {
    // world load col piece 0x4ECE90 header n1 n2 n3 n4
    world_col_piece_free(piece);
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        error = "cannot open " + filename;
        return false;
    }
    int32_t n1 = 0, n2 = 0, n3 = 0, n4 = 0;
    file.read(reinterpret_cast<char*>(&n1), sizeof(n1));
    file.read(reinterpret_cast<char*>(&n2), sizeof(n2));
    file.read(reinterpret_cast<char*>(&n3), sizeof(n3));
    file.read(reinterpret_cast<char*>(&n4), sizeof(n4));
    if (!file) {
        error = "short header in " + filename;
        return false;
    }
    if (n1 > 0) {
        piece.zoneAnchors.resize(static_cast<size_t>(n1));
        file.read(reinterpret_cast<char*>(piece.zoneAnchors.data()),
                  static_cast<std::streamsize>(n1) * static_cast<std::streamsize>(sizeof(piece.zoneAnchors[0])));
        if (!file) {
            error = "short zone anchor array in " + filename;
            return false;
        }
    }
    if (n2 != 0) {
        piece.cells.resize(static_cast<size_t>(n2));
        file.read(reinterpret_cast<char*>(piece.cells.data()),
                  static_cast<std::streamsize>(n2) * static_cast<std::streamsize>(sizeof(ColCell)));
        if (!file) {
            error = "short cell array in " + filename;
            return false;
        }
    }
    if (n3 != 0) {
        piece.edges.resize(static_cast<size_t>(n3));
        file.read(reinterpret_cast<char*>(piece.edges.data()),
                  static_cast<std::streamsize>(n3) * static_cast<std::streamsize>(sizeof(ColEdge)));
        if (!file) {
            error = "short edge array in " + filename;
            return false;
        }
    }
    if (n4 > 0) {
        piece.unreadD.resize(static_cast<size_t>(n4));
        file.read(reinterpret_cast<char*>(piece.unreadD.data()),
                  static_cast<std::streamsize>(n4) * static_cast<std::streamsize>(sizeof(piece.unreadD[0])));
        if (!file) {
            error = "short vertex array in " + filename;
            return false;
        }
    }
    return true;
}

bool world_load_track_pieces(ColTrack& track, const std::string& folder, std::string& error) {
    // world load track pieces 0x485580 track1 to track8 col
    track.pieces.clear();
    ColPiece base;
    if (!world_load_col_piece(base, folder + "/track.col", error)) {
        return false;
    }
    track.pieces.push_back(std::move(base));
    for (int i = 1; i <= 8; ++i) {
        ColPiece extra;
        std::string extraError;
        if (!world_load_col_piece(extra, folder + "/track" + std::to_string(i) + ".col", extraError)) {
            break;
        }
        track.pieces.push_back(std::move(extra));
    }
    return true;
}

// bsp walk ---------------------------------------------------------------

bool world_bsp_set_piece(BspQuery& ctx, const ColPiece& piece, float x, float z) {
    // world bsp set piece 0x4EBCF0 linear scan 3 edges
    ctx.piece = &piece;
    for (const ColCell& cell : piece.cells) {
        bool inside = true;
        for (int e = 0; e < 3; ++e) {
            uint16_t raw = cell.edge[e];
            size_t idx = static_cast<size_t>(raw & 0x7fffu);
            if (idx >= piece.edges.size()) {
                inside = false;
                break;
            }
            const ColEdge& edge = piece.edges[idx];
            bool negate = static_cast<int16_t>(raw) < 0;
            float value = negate ? (-(x * edge.a) - z * edge.b - edge.c) : (x * edge.a + z * edge.b + edge.c);
            if (value < kEpsilon) {
                inside = false;
                break;
            }
        }
        if (inside) {
            ctx.cell = &cell;
            ctx.x = x;
            ctx.z = z;
            return true;
        }
    }
    return false;
}

int world_bsp_edge_sweep(BspQuery& ctx, float x, float z, float* outT) {
    // world bsp edge sweep 0x4EBDD0 earliest crossing cell 3 edges
    *outT = kSweepBound;
    int best = -1;
    if (!ctx.cell || !ctx.piece) {
        return best;
    }
    const ColCell& cell = *ctx.cell;
    for (int e = 0; e < 3; ++e) {
        uint16_t raw = cell.edge[e];
        size_t idx = static_cast<size_t>(raw & 0x7fffu);
        if (idx >= ctx.piece->edges.size()) {
            continue;
        }
        const ColEdge& edge = ctx.piece->edges[idx];
        bool negate = static_cast<int16_t>(raw) < 0;
        float newVal = negate ? (-(x * edge.a) - z * edge.b - edge.c) : (x * edge.a + z * edge.b + edge.c);
        if (newVal < kEpsilon) {
            float oldVal = negate ? (-(ctx.x * edge.a) - ctx.z * edge.b - edge.c)
                                   : (ctx.x * edge.a + ctx.z * edge.b + edge.c);
            float t = oldVal / (oldVal - newVal);
            if (e < 2) {
                if (t < *outT) {
                    *outT = t;
                    best = static_cast<int16_t>(raw);
                }
            } else if (t < *outT) {
                *outT = t;
                return static_cast<int16_t>(raw); // the third edge returns as soon as it wins
            }
        }
    }
    return best;
}

bool world_bsp_locate_point(BspQuery& ctx, float x, float z, const ColCell** outCell, float* outT) {
    // world bsp locate point 0x4EBFC0 walks tree segment old to x z
    ctx.prevX = ctx.x;
    ctx.prevZ = ctx.z;
    ctx.prevCell = ctx.cell;
    float t = kSweepBound;
    int steps = 0;
    bool result = false;
    while (ctx.piece) {
        int edgeIdx = world_bsp_edge_sweep(ctx, x, z, &t);
        if (edgeIdx == -1) {
            ctx.x = x;
            ctx.z = z;
            result = true;
            break;
        }
        size_t rawIdx = static_cast<size_t>(edgeIdx & 0x7fff);
        if (rawIdx >= ctx.piece->edges.size()) {
            result = false;
            break;
        }
        const ColEdge& edge = ctx.piece->edges[rawIdx];
        int child = (edgeIdx < 0) ? edge.childBehind : edge.childFront;
        uint16_t childU = static_cast<uint16_t>(child);
        bool deadEnd = (childU == 0xffffu) || (ctx.piece->cells.empty()) ||
                       (static_cast<size_t>(childU) >= ctx.piece->cells.size());
        if (deadEnd) {
            float fromX = ctx.prevX, fromZ = ctx.prevZ;
            ctx.cell = ctx.prevCell;
            ctx.x = (x - fromX) * t + fromX;
            ctx.z = (z - fromZ) * t + fromZ;
            ctx.lastEdge = &edge;
            result = false;
            break;
        }
        ctx.cell = &ctx.piece->cells[childU];
        ++steps;
        if (steps > kBspStepCap) {
            result = false;
            break;
        }
    }
    if (outCell) {
        *outCell = ctx.cell;
    }
    if (outT) {
        *outT = t;
    }
    return result;
}

const char* world_query_surface_name(const BspQuery& ctx) {
    // world query surface name 0x4EC0B0 upper case cell name
    if (!ctx.cell) {
        return nullptr;
    }
    size_t i = 0;
    for (; i < sizeof(ctx.cell->surfaceName) && ctx.cell->surfaceName[i] != '\0'; ++i) {
        char c = ctx.cell->surfaceName[i];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        ctx.surfaceName[i] = c;
    }
    ctx.surfaceName[i] = '\0';
    return ctx.surfaceName;
}

// height and ground ---------------------------------------------------------------

bool world_locate_piece_by_height(BspQuery& ctx, const ColTrack& track, float x, float z, float yHint,
                                   float* outY, int* outSurfaceIndex) {
    // world locate piece height 0x4857A0 A x plus B z
    float nx = -x, nz = -z;
    for (size_t i = 1; i < track.pieces.size(); ++i) {
        const ColPiece& piece = track.pieces[i];
        ctx.piece = &piece;
        if (!world_bsp_set_piece(ctx, piece, nx, nz)) {
            continue;
        }
        if (!world_bsp_locate_point(ctx, nx, nz) || !ctx.cell) {
            continue;
        }
        float height = ctx.cell->heightA * x + ctx.cell->heightB * z - ctx.cell->heightD;
        if (yHint - kHeightTolerance < height && height < yHint + kHeightTolerance) {
            if (outY) {
                *outY = height;
            }
            if (outSurfaceIndex) {
                *outSurfaceIndex = world_surface_name_to_index(world_query_surface_name(ctx));
            }
            return true;
        }
    }
    if (track.pieces.empty()) {
        return false;
    }
    ctx.piece = &track.pieces[0];
    bool found = world_bsp_set_piece(ctx, track.pieces[0], nx, nz);
    if (found && ctx.cell) {
        float height = ctx.cell->heightA * x + ctx.cell->heightB * z - ctx.cell->heightD;
        if (outY) {
            *outY = height;
        }
        if (outSurfaceIndex) {
            *outSurfaceIndex = world_surface_name_to_index(world_query_surface_name(ctx));
        }
    }
    return found;
}

bool world_ground_test_point(BspQuery& ctx, float x, float z) {
    // world ground test point 0x4858E0 tree walk scan fallback
    if (!ctx.active) {
        return false;
    }
    float nx = -x, nz = -z;
    world_bsp_locate_point(ctx, nx, nz);
    bool found = world_bsp_locate_point(ctx, nx, nz);
    if (!found) {
        if (!ctx.pieceCache) {
            return false;
        }
        found = world_bsp_set_piece(ctx, *ctx.pieceCache, nx, nz);
        if (!found) {
            return false;
        }
    }
    return true;
}

bool world_place_probe_local(BspQuery& ctx, const ColTrack& track, float x, float z, float y, const ColPiece* piece,
                             bool worldReady) {
    // world place probe local 0x486300 clears 0x9c 0x08 0x90 0x94 0x98 binds piece 0 at 0x4c
    ctx.active = false;
    ctx.lastEdge = nullptr;
    ctx.pieceIndexValid = false;
    ctx.cachedPieceIndex = 0;
    if (track.pieces.empty()) return false;
    ctx.pieceCache = &track.pieces[0];
    world_bsp_set_piece(ctx, track.pieces[0], -x, -z);
    if (!worldReady && piece != nullptr) {
        if (piece->cells.empty()) return false;
        ctx.pieceCache = piece;
        if (!world_bsp_set_piece(ctx, *piece, -x, -z)) return false;
    } else {
        float outY = 0.0f;
        int surface = 0;
        if (!world_locate_piece_by_height(ctx, track, x, z, y, &outY, &surface)) return false;
    }
    ctx.active = true;
    return true;
}

// wheel probes ---------------------------------------------------------------

const char* world_wheel_query_surface(BspQuery& ctx, float wheelX, float wheelZ) {
    // world wheel query surface 0x4A0680 negated x z
    world_bsp_locate_point(ctx, -wheelX, -wheelZ);
    return world_query_surface_name(ctx);
}

int world_wheel_surface_index(BspQuery& ctx, float wheelX, float wheelZ, bool trackActive) {
    // world wheel surface index 0x4A1310 minus 1 track not active
    if (!trackActive) {
        return -1;
    }
    return world_surface_name_to_index(world_wheel_query_surface(ctx, wheelX, wheelZ));
}

bool world_probe_wheel_ground(BspQuery& ctx, const ColTrack& track, const float footprintX[4],
                              const float footprintZ[4], int wheelIndex) {
    // world probe wheel ground 0x486570 the four probe points of car 0x3274 must all land in a cell first
    if (!ctx.active) {
        return false;
    }
    int landed = 0;
    for (int i = 0; i < 4; ++i) {
        if (world_bsp_locate_point(ctx, -footprintX[i], -footprintZ[i])) {
            ++landed;
        }
    }
    if (landed < 4) {
        return false;
    }
    if (wheelIndex < 0 || wheelIndex > 3) {
        return false;
    }
    float wheelX = footprintX[wheelIndex];
    float wheelZ = footprintZ[wheelIndex];
    const ColPiece* before = ctx.pieceCache;
    const char* name = world_wheel_query_surface(ctx, wheelX, wheelZ);
    int parsedIndex = -1;
    if (name) {
        parsedIndex = -2;
        const char* hash = std::strchr(name, '#');
        if (hash) {
            int n = -1;
            if (std::sscanf(hash + 1, "%d", &n) == 1) {
                parsedIndex = n - 1;
            }
        }
        if (parsedIndex >= 0 && parsedIndex != ctx.cachedPieceIndex) {
            ctx.pieceIndexValid = false;
        }
    }
    size_t extraCount = track.pieces.empty() ? 0u : track.pieces.size() - 1;
    if (!ctx.pieceIndexValid && parsedIndex >= 0 && static_cast<size_t>(parsedIndex) < extraCount) {
        const ColPiece& target = track.pieces[static_cast<size_t>(parsedIndex) + 1];
        ctx.pieceCache = &target;
        if (!world_bsp_set_piece(ctx, target, -wheelX, -wheelZ)) {
            return false;
        }
        ctx.cachedPieceIndex = parsedIndex;
        ctx.pieceIndexValid = true;
    }
    return ctx.pieceCache != before;
}

bool world_ground_height_at(BspQuery& ctx, float p[3]) {
    // world ground height at 0x485970 the found cell plane on minus x minus y z minus dot minus D
    if (!ctx.active) {
        return false;
    }
    if (!world_ground_test_point(ctx, p[0], p[1])) {
        return false;
    }
    const ColCell* cell = ctx.cell;
    if (!cell) {
        return false;
    }
    float dot = cell->heightA * (-p[0]) + cell->heightB * (-p[1]) + cell->heightC * p[2];
    p[2] = p[2] - (dot + cell->heightD);
    return true;
}

// track flow ---------------------------------------------------------------

bool world_on_track_check(int progress) {
    // world on track check 0x4A0750 on track 100 to 299
    return progress < 100 || progress > 299;
}

bool world_car_overlap_test(const CarPose& a, const CarPose& b) {
    // world car overlap test 0x497EE0 math hypot2d of the plane delta under 10 and under 0 8 times the extents
    float dist = std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    if (dist > kOverlapMaxDistance) {
        return false;
    }
    return dist < (a.extentY + b.extentY) * kOverlapExtentScale;
}

int world_car_find_overlap(const CarPose* cars, int count, int selfIndex) {
    // world car find overlap 0x497F80 height overlap of the two z extents then the plane test
    if (selfIndex < 0 || selfIndex >= count) {
        return -1;
    }
    const CarPose& self = cars[selfIndex];
    for (int i = 0; i < count && i < 30; ++i) {
        const CarPose& other = cars[i];
        if (i == selfIndex || other.slotOccupied == 0 || other.finished == 1) {
            continue;
        }
        if (self.z <= other.extentZ + other.z && other.z <= self.extentZ + self.z) {
            if (world_car_overlap_test(self, other)) {
                return i;
            }
        }
    }
    return -1;
}

bool world_car_push_apart(const CarPose& self, CarPose& other, float outDir[3], int* outEffectCar,
                          int* outEffectTier) {
    // world car push apart 0x498800 after the overlap D3DXVec3Normalize of other minus self
    if (outEffectTier) {
        *outEffectTier = -1;
    }
    if (outEffectCar) {
        *outEffectCar = -1;
    }
    float dx = other.x - self.x;
    float dy = other.y - self.y;
    float dz = other.z - self.z;
    float lenSq = dx * dx + dy * dy + dz * dz;
    if (lenSq > 0.0f) {
        float invLen = 1.0f / std::sqrt(lenSq);
        dx *= invLen;
        dy *= invLen;
        dz *= invLen;
    }
    outDir[0] = dx;
    outDir[1] = dy;
    outDir[2] = dz;
    // 0x4988BC the other car plane position moves by 0 8 dx and 0 8 dy times 0 06
    other.x += dx * kPushApartDirScale * kPushApartNudge;
    other.y += dy * kPushApartDirScale * kPushApartNudge;
    // 0x4988EC the faster car ties to the other plays the impact tier 0 over 30 tier 1 over 15
    bool selfFaster = self.speed > other.speed;
    float speed = selfFaster ? self.speed : other.speed;
    if (outEffectCar) {
        *outEffectCar = selfFaster ? 0 : 1;
    }
    if (outEffectTier) {
        if (speed > kPushApartHighSpeed) {
            *outEffectTier = 0;
        } else if (speed > kPushApartLowSpeed) {
            *outEffectTier = 1;
        }
    }
    return true;
}

bool world_stuck_probe(BspQuery& ctx, float x, float z, float headingDeg) {
    // world stuck probe 0x498590 fans points around the heading through FUN 0044DEC0 not a height search
    float outerBase = headingDeg - kStuckOuterCenter;
    for (float outer = -kStuckOuterBound; outer <= kStuckOuterBound; outer += kStuckOuterStep) {
        float angleSrc = outerBase + outer;
        float angleRad = -(angleSrc - kStuckOuterCenter) * kDegToRad; // FUN 0x0044DEC0
        float cosA = std::cos(angleRad);
        float sinA = std::sin(angleRad);
        for (float radius = 0.0f; radius < kStuckInnerBound; radius += kStuckInnerStep) {
            float px = cosA * radius + x;
            float pz = sinA * radius + z;
            if (!world_ground_test_point(ctx, px, pz)) {
                return true;
            }
            const char* surf = world_query_surface_name(ctx);
            if (surf && _stricmp(surf, "PUSH") == 0) {
                return true;
            }
        }
    }
    return false;
}

// surface tags ---------------------------------------------------------------

void world_decode_surface_tag_bytes(char outTag[5]) {
    // world decode surface tag bytes 0x487280 dest src minus plus 1
    for (int i = 0; i < 4; ++i) {
        outTag[i] = static_cast<char>(kRegeSourceBytes[i] - (i + 1));
    }
    outTag[4] = '\0';
}

void world_decode_surface_tag_digits(char outTag[5]) {
    // world decode surface tag digits 0x487450 dest src over 37
    for (int i = 0; i < 4; ++i) {
        outTag[i] = static_cast<char>(kLavaSourceDigits[i] / 37 - (i + 1));
    }
    outTag[4] = '\0';
}

int world_wheel_surface_tag_count(BspQuery& ctx, const float wheelX[4], const float wheelZ[4], const char* tag) {
    // world wheel surface tag count 0x4A1380 counts wheels surface
    if (!ctx.active || !tag) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        world_bsp_locate_point(ctx, -wheelX[i], -wheelZ[i]);
        const char* name = world_query_surface_name(ctx);
        if (name && _stricmp(name, tag) == 0) {
            ++count;
        }
    }
    return count;
}

// surfaces ---------------------------------------------------------------

int world_surface_name_to_index(const char* name) {
    // world surface name to index 0x4866D0 9 names REGEN DUST
    if (!name) {
        return -1;
    }
    if (_stricmp(name, "DUST") == 0) return 0;
    if (_stricmp(name, "ASPHALT") == 0) return 1;
    if (_stricmp(name, "WATER") == 0) return 2;
    if (_stricmp(name, "GRASS") == 0) return 3;
    if (_stricmp(name, "SNOW") == 0) return 4;
    if (_stricmp(name, "ICE") == 0) return 5;
    if (_stricmp(name, "BOARD") == 0) return 6;
    if (_stricmp(name, "WATER_REG") == 0) return 7;
    if (_stricmp(name, "GRASS_L") == 0) return 8;
    if (_stricmp(name, "REGEN") == 0) return 0;
    return -1;
}

float world_surface_friction(int index) {
    if (index < 0 || index >= 9) {
        return kDefaultFriction;
    }
    return kSurfaceTable[index].friction;
}

float world_surface_contact_drag(int index) {
    // world surface contact drag 0x486D70 the tick takes it times a quarter off the throttle per loaded wheel
    if (index < 0 || index >= 9) {
        return kDefaultGripOrPenalty;
    }
    return kSurfaceTable[index].contactDrag;
}

float world_surface_grip(int index) {
    if (index < 0 || index >= 9) {
        return kDefaultGripOrPenalty;
    }
    return kSurfaceTable[index].grip;
}

float standings_rank_throttle_multiplier(int row) {
    if (row < 0 || row >= 30) {
        return 1.0f;
    }
    return kRankThrottleTable[row];
}

// regen zones ---------------------------------------------------------------

bool world_load_regen_zones(const std::string& csvPath, std::vector<RegenZone>& zones, std::string& error) {
    // world load regen zones 0x48A910 up to 100 lines 4
    zones.clear();
    std::FILE* file = std::fopen(csvPath.c_str(), "rb");
    if (!file) {
        error = "cannot open " + csvPath;
        return false;
    }
    for (int i = 0; i < 100; ++i) {
        RegenZone zone;
        int n = std::fscanf(file, "%f,%f,%f,%f", &zone.x, &zone.y, &zone.z, &zone.radius);
        if (n != 4) {
            break;
        }
        zones.push_back(zone);
    }
    std::fclose(file);
    return true;
}

}
