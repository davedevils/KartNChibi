// Console list extract and pack tool for a pak dat archive no window needed
#include "PakReader.h"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// Opens either one pak file or a game folder holding pak001 dat and the rest
bool open_source(KnC::PakReader& reader, const std::string& path) {
    if (fs::is_directory(path)) {
        return reader.OpenGameDir(path);
    }
    return reader.Open(path);
}

int run_list(const std::string& source) {
    KnC::PakReader reader;
    if (!open_source(reader, source)) {
        std::printf("could not open %s\n", source.c_str());
        return 1;
    }
    const auto& entries = reader.GetEntries();
    for (const auto& entry : entries) {
        std::printf("%s size %u pak %d\n", entry.path.c_str(), entry.size, entry.pakIndex + 1);
    }
    std::printf("total files %zu\n", entries.size());
    return 0;
}

int run_extract(const std::string& source, const std::string& outDir) {
    KnC::PakReader reader;
    if (!open_source(reader, source)) {
        std::printf("could not open %s\n", source.c_str());
        return 1;
    }
    const auto& entries = reader.GetEntries();
    size_t written = 0;
    for (const auto& entry : entries) {
        std::vector<uint8_t> data = reader.ReadPath(entry.path);
        if (data.empty() && entry.size != 0) {
            std::printf("skip unreadable %s\n", entry.path.c_str());
            continue;
        }
        std::string relative = entry.path;
        for (char& c : relative) {
            if (c == '\\') {
                c = '/';
            }
        }
        fs::path outPath = fs::path(outDir) / relative;
        std::error_code ec;
        fs::create_directories(outPath.parent_path(), ec);
        std::ofstream out(outPath, std::ios::binary);
        if (!out) {
            std::printf("cannot write %s\n", outPath.string().c_str());
            continue;
        }
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        ++written;
    }
    std::printf("extracted %zu of %zu files\n", written, entries.size());
    return 0;
}

// Writes one archive using the entry layout the reader already decodes
bool pack_directory(const std::string& outPak, const std::string& inputDir, std::string& error) {
    std::vector<fs::path> files;
    for (const auto& item : fs::recursive_directory_iterator(inputDir)) {
        if (item.is_regular_file()) {
            files.push_back(item.path());
        }
    }
    std::ofstream out(outPak, std::ios::binary);
    if (!out) {
        error = "cannot create " + outPak;
        return false;
    }

    char header[32] = {};
    std::memcpy(header, "NKZIP", 5);
    uint32_t version = 1;
    uint32_t fileCount = static_cast<uint32_t>(files.size());
    std::memcpy(header + 8, &version, sizeof(version));
    std::memcpy(header + 12, &fileCount, sizeof(fileCount));
    out.write(header, sizeof(header));

    for (const auto& path : files) {
        std::string relative = fs::relative(path, inputDir).generic_string();
        std::string entryPath = "./" + relative;
        if (entryPath.size() >= 260) {
            error = "path too long " + entryPath;
            return false;
        }

        std::ifstream in(path, std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        uint32_t size = static_cast<uint32_t>(data.size());

        char entryHeader[12] = {};
        std::memcpy(entryHeader + 8, &size, sizeof(size));
        out.write(entryHeader, sizeof(entryHeader));

        char pathField[260] = {};
        std::memcpy(pathField, entryPath.data(), entryPath.size());
        out.write(pathField, sizeof(pathField));

        if (!data.empty()) {
            out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
    }
    return true;
}

int run_pack(const std::string& outPak, const std::string& inputDir) {
    std::string error;
    if (!pack_directory(outPak, inputDir, error)) {
        std::printf("pack failed %s\n", error.c_str());
        return 1;
    }
    std::printf("packed %s from %s\n", outPak.c_str(), inputDir.c_str());
    return 0;
}

void print_usage() {
    std::printf("usage pak_tool list <pak file or game dir>\n");
    std::printf("usage pak_tool extract <pak file or game dir> <out dir>\n");
    std::printf("usage pak_tool pack <out pak file> <input dir>\n");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    std::string command = argv[1];
    if (command == "list" && argc >= 3) {
        return run_list(argv[2]);
    }
    if (command == "extract" && argc >= 4) {
        return run_extract(argv[2], argv[3]);
    }
    if (command == "pack" && argc >= 4) {
        return run_pack(argv[2], argv[3]);
    }
    print_usage();
    return 1;
}
