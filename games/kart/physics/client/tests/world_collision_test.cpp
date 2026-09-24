// Proves world collision against real Cookie 01 data prints counts fails only on real defect
#include "../world_collision.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace KnC::Kart::Client;

namespace {

std::string clientDataRoot() {
    // KNC CLIENT DATA overrides default stock client data folder per client physics README
    if (const char* env = std::getenv("KNC_CLIENT_DATA")) {
        return env;
    }
    return "Data";
}

// start ini rows are x y z heading settled by the Race 01 ghost zero drives toward minus x
bool readStartPoints(const std::string& path, std::vector<std::array<float, 3>>& points) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        std::string field;
        std::array<float, 3> xyz{};
        int count = 0;
        while (count < 3 && std::getline(row, field, ',')) {
            xyz[static_cast<size_t>(count)] = std::strtof(field.c_str(), nullptr);
            ++count;
        }
        if (count == 3) {
            points.push_back(xyz);
        }
    }
    return true;
}

} // namespace

int main() {
    std::string dataRoot = clientDataRoot();
    if (!std::filesystem::exists(dataRoot)) {
        std::printf("client data folder missing at %s, skipping\n", dataRoot.c_str());
        return 0;
    }

    std::string folder = dataRoot + "/Public/World/Cookie/Cookie_01";
    if (!std::filesystem::exists(folder)) {
        std::printf("cookie 01 track folder missing at %s, skipping\n", folder.c_str());
        return 0;
    }

    ColTrack track;
    std::string error;
    if (!world_load_track_pieces(track, folder, error)) {
        std::printf("world_load_track_pieces failed, %s\n", error.c_str());
        return 1;
    }

    size_t totalCells = 0, totalEdges = 0;
    for (size_t i = 0; i < track.pieces.size(); ++i) {
        const ColPiece& piece = track.pieces[i];
        std::printf("piece %zu, cells %zu, edges %zu, zone anchors %zu, vertices %zu\n", i, piece.cells.size(),
                     piece.edges.size(), piece.zoneAnchors.size(), piece.unreadD.size());
        totalCells += piece.cells.size();
        totalEdges += piece.edges.size();
    }
    std::printf("track pieces %zu, total cells %zu, total edges %zu\n", track.pieces.size(), totalCells,
                totalEdges);

    // every cell 3 edge indices must land inside piece own edge array real loader invariant
    size_t badEdgeIndex = 0;
    size_t mappedSurface = 0, unmappedSurface = 0;
    for (const ColPiece& piece : track.pieces) {
        for (const ColCell& cell : piece.cells) {
            for (uint16_t raw : cell.edge) {
                size_t idx = static_cast<size_t>(raw & 0x7fffu);
                if (idx >= piece.edges.size()) {
                    ++badEdgeIndex;
                }
            }
            int surfaceIndex = world_surface_name_to_index(cell.surfaceName);
            if (surfaceIndex >= 0) {
                ++mappedSurface;
            } else {
                ++unmappedSurface;
            }
        }
    }
    std::printf("cells with an edge index out of range: %zu\n", badEdgeIndex);
    std::printf("cells with a surface name in the nine, %zu, not in the nine, %zu\n", mappedSurface,
                unmappedSurface);
    if (badEdgeIndex != 0) {
        std::printf("a cell edge index points outside the edge array, loader defect\n");
        return 1;
    }

    std::vector<std::array<float, 3>> points;
    std::string startPath = folder + "/start.ini";
    if (!readStartPoints(startPath, points)) {
        std::printf("start.ini missing at %s, skipping the locate pass\n", startPath.c_str());
        return 0;
    }
    std::printf("start.ini rows read: %zu\n", points.size());

    bool anyMissed = false;
    BspQuery ctx;
    for (size_t i = 0; i < points.size(); ++i) {
        float x = points[i][0], y = points[i][1], z = points[i][2];
        float outY = 0.0f;
        int outSurface = -1;
        bool found = world_locate_piece_by_height(ctx, track, x, z, y, &outY, &outSurface);
        if (found) {
            std::printf("point %zu, x %.3f y %.3f z %.3f, height plane %.3f, surface index %d\n", i, x, y, z,
                        outY, outSurface);
        } else {
            std::printf("point %zu, x %.3f y %.3f z %.3f, locate failed\n", i, x, y, z);
            anyMissed = true;
        }
    }

    if (anyMissed) {
        std::printf("at least one start.ini point did not locate on the track\n");
        return 1;
    }

    std::printf("world_collision test passed\n");
    return 0;
}
