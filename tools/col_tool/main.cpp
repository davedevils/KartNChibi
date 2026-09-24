// Console tool that dumps a track collision folder using the kart client world collision port
#include "world_collision.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

using namespace KnC::Kart::Client;

namespace {

const char* kSurfaceNames[9] = {
    "DUST", "ASPHALT", "WATER", "GRASS", "SNOW", "ICE", "BOARD", "WATER_REG", "GRASS_L",
};

// Reads the surface name through the upper case getter the engine itself uses
std::string cell_surface_name(const ColCell& cell) {
    BspQuery temp;
    temp.cell = &cell;
    const char* name = world_query_surface_name(temp);
    return name ? std::string(name) : std::string();
}

// Prints one piece cell edge zone anchor and vertex counts then cells per surface name
void print_piece_summary(size_t index, const ColPiece& piece) {
    std::printf("piece %zu cells %zu edges %zu zone anchors %zu vertices %zu\n", index, piece.cells.size(),
                piece.edges.size(), piece.zoneAnchors.size(), piece.unreadD.size());
    std::map<std::string, int> counts;
    for (const ColCell& cell : piece.cells) {
        counts[cell_surface_name(cell)]++;
    }
    for (const auto& row : counts) {
        std::printf("  surface %s cells %d\n", row.first.c_str(), row.second);
    }
}

// Prints every cell of one piece with its plane edges and surface name
void print_cells(size_t index, const ColPiece& piece) {
    for (size_t c = 0; c < piece.cells.size(); ++c) {
        const ColCell& cell = piece.cells[c];
        std::printf("piece %zu cell %zu plane %.4f %.4f %.4f %.4f edges %u %u %u name %s\n", index, c,
                    cell.heightA, cell.heightB, cell.heightC, cell.heightD,
                    static_cast<unsigned>(cell.edge[0]), static_cast<unsigned>(cell.edge[1]),
                    static_cast<unsigned>(cell.edge[2]), cell_surface_name(cell).c_str());
    }
}

// Finds the cell under a point using the same piece walk as the height locate call
bool find_cell_at(const ColTrack& track, float x, float z, BspQuery& ctx, size_t* outPiece) {
    float nx = -x;
    float nz = -z;
    for (size_t i = 1; i < track.pieces.size(); ++i) {
        const ColPiece& piece = track.pieces[i];
        if (!world_bsp_set_piece(ctx, piece, nx, nz)) {
            continue;
        }
        if (world_bsp_locate_point(ctx, nx, nz) && ctx.cell) {
            *outPiece = i;
            return true;
        }
    }
    if (!track.pieces.empty() && world_bsp_set_piece(ctx, track.pieces[0], nx, nz) && ctx.cell) {
        *outPiece = 0;
        return true;
    }
    return false;
}

// Prints the cell under a point its edges its surface and the ground height and grip
void print_at(const ColTrack& track, float x, float z) {
    BspQuery ctx;
    size_t pieceIndex = 0;
    if (!find_cell_at(track, x, z, ctx, &pieceIndex) || !ctx.cell) {
        std::printf("at %.3f %.3f no cell found\n", x, z);
        return;
    }
    const ColCell& cell = *ctx.cell;
    float height = cell.heightA * x + cell.heightB * z - cell.heightD;
    std::string name = cell_surface_name(cell);
    int surfaceIndex = world_surface_name_to_index(name.c_str());
    float friction = world_surface_friction(surfaceIndex);
    float grip = world_surface_grip(surfaceIndex);
    float contactDrag = world_surface_contact_drag(surfaceIndex);
    std::printf("at %.3f %.3f piece %zu edges %u %u %u surface %s index %d height %.4f friction %.4f"
                " grip %.4f contact drag %.4f\n",
                x, z, pieceIndex, static_cast<unsigned>(cell.edge[0]), static_cast<unsigned>(cell.edge[1]),
                static_cast<unsigned>(cell.edge[2]), name.c_str(), surfaceIndex, height, friction, grip,
                contactDrag);
}

// Prints every row loaded from the regen file for this track folder
void print_regen(const std::string& folder) {
    std::vector<RegenZone> zones;
    std::string error;
    if (!world_load_regen_zones(folder + "/regen.ini", zones, error)) {
        std::printf("regen ini load failed %s\n", error.c_str());
        return;
    }
    std::printf("regen zones %zu\n", zones.size());
    for (size_t i = 0; i < zones.size(); ++i) {
        const RegenZone& zone = zones[i];
        std::printf("  row %zu x %.3f y %.3f z %.3f radius %.3f\n", i, zone.x, zone.y, zone.z, zone.radius);
    }
}

// Prints friction grip and contact drag for the nine known surface names
void print_surfaces() {
    std::printf("surface table\n");
    for (int i = 0; i < 9; ++i) {
        std::printf("  %d %s friction %.4f grip %.4f contact drag %.4f\n", i, kSurfaceNames[i],
                    world_surface_friction(i), world_surface_grip(i), world_surface_contact_drag(i));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage col_tool <track folder> [--at x z] [--regen] [--cells] [--surfaces]\n");
        return 1;
    }
    std::string folder = argv[1];
    bool wantAt = false;
    bool wantRegen = false;
    bool wantCells = false;
    bool wantSurfaces = false;
    float atX = 0.0f;
    float atZ = 0.0f;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--at" && i + 2 < argc) {
            wantAt = true;
            atX = std::strtof(argv[++i], nullptr);
            atZ = std::strtof(argv[++i], nullptr);
        } else if (arg == "--regen") {
            wantRegen = true;
        } else if (arg == "--cells") {
            wantCells = true;
        } else if (arg == "--surfaces") {
            wantSurfaces = true;
        }
    }

    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, folder, error)) {
        std::printf("world_load_track_pieces failed %s\n", error.c_str());
        return 1;
    }

    for (size_t i = 0; i < track.pieces.size(); ++i) {
        print_piece_summary(i, track.pieces[i]);
    }
    if (wantCells) {
        for (size_t i = 0; i < track.pieces.size(); ++i) {
            print_cells(i, track.pieces[i]);
        }
    }
    if (wantAt) {
        print_at(track, atX, atZ);
    }
    if (wantRegen) {
        print_regen(folder);
    }
    if (wantSurfaces) {
        print_surfaces();
    }
    return 0;
}
