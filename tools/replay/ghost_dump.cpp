// Dumps a ghost file or a live ghost record row as text or csv see the usage in main

#include "ghost_replay.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

constexpr float kYawByteToDeg = 360.0f / 255.0f;

void print_usage() {
    std::printf("ghost_dump <ghost file> [--csv]\n");
    std::printf("ghost_dump --db <host> <port> <user> <password> <database> "
               "<track id> <char id> [--save <file>]\n");
}

void print_header(const KnC::Tools::GhostRecording& rec) {
    std::printf("name %s\n", rec.name.c_str());
    std::printf("track %d char %u time ms %d car kind %u\n", rec.trackId, rec.charId,
               rec.timeMs, rec.carKind);
    std::printf("stored frame count %u loaded samples %zu\n", rec.frameCount, rec.samples.size());
}

void print_samples(const KnC::Tools::GhostRecording& rec, bool csv) {
    if (csv) {
        std::printf("index,x,y,z,yaw,flags,nibbles,input\n");
    }
    for (size_t i = 0; i < rec.samples.size(); ++i) {
        const auto& s = rec.samples[i];
        const float yaw = static_cast<float>(s.yawByte) * kYawByteToDeg;
        const unsigned flags = s.flags;
        const unsigned nibbles = s.nibbles;
        const unsigned input = s.inputMask;
        if (csv) {
            std::printf("%zu,%f,%f,%f,%f,0x%02X,0x%02X,0x%02X\n", i,
                       static_cast<double>(s.pos[0]), static_cast<double>(s.pos[1]),
                       static_cast<double>(s.pos[2]), static_cast<double>(yaw),
                       flags, nibbles, input);
        } else {
            std::printf("%6zu  x %9.3f  y %9.3f  z %9.3f  yaw %7.2f  "
                       "flags 0x%02X  nibbles 0x%02X  input 0x%02X\n", i,
                       static_cast<double>(s.pos[0]), static_cast<double>(s.pos[1]),
                       static_cast<double>(s.pos[2]), static_cast<double>(yaw),
                       flags, nibbles, input);
        }
    }
}

int run_file_dump(const std::string& path, bool csv) {
    KnC::Tools::GhostRecording rec;
    std::string error;
    if (!KnC::Tools::load_ghost_file(path, rec, error)) {
        std::fprintf(stderr, "load failed %s\n", error.c_str());
        return 1;
    }
    print_header(rec);
    print_samples(rec, csv);
    return 0;
}

int run_db_dump(int argc, char** argv) {
    // argv layout after dash dash db is host port user password database track id char id
    if (argc < 9) {
        print_usage();
        return 1;
    }
    const std::string host = argv[2];
    const uint16_t port = static_cast<uint16_t>(std::atoi(argv[3]));
    const std::string user = argv[4];
    const std::string password = argv[5];
    const std::string database = argv[6];
    const int32_t trackId = static_cast<int32_t>(std::atoi(argv[7]));
    const uint32_t charId = static_cast<uint32_t>(std::strtoul(argv[8], nullptr, 10));

    std::string savePath;
    for (int i = 9; i < argc; ++i) {
        if (std::string(argv[i]) == "--save" && i + 1 < argc) savePath = argv[i + 1];
    }

    KnC::Tools::GhostRecording rec;
    std::string error;
    if (!KnC::Tools::load_ghost_db(host, port, user, password, database, trackId, charId, rec, error)) {
        std::fprintf(stderr, "database load failed %s\n", error.c_str());
        return 1;
    }
    print_header(rec);

    if (!savePath.empty()) {
        std::string saveError;
        if (!KnC::Tools::save_ghost_file(savePath, rec, saveError)) {
            std::fprintf(stderr, "save failed %s\n", saveError.c_str());
            return 1;
        }
        std::printf("saved %s\n", savePath.c_str());
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    const std::string first = argv[1];
    if (first == "--db") return run_db_dump(argc, argv);

    bool csv = false;
    for (int i = 2; i < argc; ++i) {
        if (std::string(argv[i]) == "--csv") csv = true;
    }
    return run_file_dump(first, csv);
}
