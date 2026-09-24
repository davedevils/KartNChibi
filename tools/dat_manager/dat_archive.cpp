#include "dat_archive.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace KnC::Tools {

namespace {

long process_id() {
#if defined(_WIN32)
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

constexpr uint32_t kColourImage  = 0xff64b464u;
constexpr uint32_t kColourModel  = 0xff648cb4u;
constexpr uint32_t kColourAudio  = 0xffc88c64u;
constexpr uint32_t kColourText   = 0xffb4b4b4u;
constexpr uint32_t kColourCar    = 0xff50b4ffu;
constexpr uint32_t kColourOther  = 0xff968c8cu;

constexpr const char* kSettingsFile = "dat_manager.ini";

std::string extension_of(const std::string& name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos) return std::string();
    return lower_text(name.substr(dot));
}

bool write_bytes(const fs::path& path, const std::vector<uint8_t>& bytes) {
    std::error_code ignored;
    fs::create_directories(path.parent_path(), ignored);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(out);
}

}

FileKind file_kind(const std::string& name) {
    const std::string ext = extension_of(name);
    if (ext.empty()) return FileKind::Unknown;
    if (ext == ".dds") return FileKind::Dds;
    if (ext == ".png") return FileKind::Png;
    if (ext == ".bmp") return FileKind::Bmp;
    if (ext == ".jpg" || ext == ".jpeg") return FileKind::Jpg;
    if (ext == ".tga") return FileKind::Tga;
    if (ext == ".nif") return FileKind::Nif;
    if (ext == ".kf") return FileKind::Kf;
    if (ext == ".kfm") return FileKind::Kfm;
    if (ext == ".wav") return FileKind::Wav;
    if (ext == ".ogg") return FileKind::Ogg;
    if (ext == ".mp3") return FileKind::Mp3;
    if (ext == ".xml") return FileKind::Xml;
    if (ext == ".txt") return FileKind::Txt;
    if (ext == ".ini") return FileKind::Ini;
    if (ext == ".csv") return FileKind::Csv;
    if (ext == ".lua") return FileKind::Lua;
    if (ext == ".car") return FileKind::Car;
    if (ext == ".ifl") return FileKind::Ifl;
    if (ext == ".col") return FileKind::Col;
    return FileKind::Other;
}

const char* kind_name(FileKind kind) {
    switch (kind) {
    case FileKind::Dds: return "DDS Texture";
    case FileKind::Png: return "PNG Image";
    case FileKind::Bmp: return "BMP Image";
    case FileKind::Jpg: return "JPEG Image";
    case FileKind::Tga: return "TGA Image";
    case FileKind::Nif: return "NIF Model";
    case FileKind::Kf:  return "KF Animation";
    case FileKind::Kfm: return "KFM Clip List";
    case FileKind::Wav: return "WAV Audio";
    case FileKind::Ogg: return "OGG Audio";
    case FileKind::Mp3: return "MP3 Audio";
    case FileKind::Xml: return "XML Config";
    case FileKind::Txt: return "Text File";
    case FileKind::Ini: return "INI Config";
    case FileKind::Csv: return "CSV Data";
    case FileKind::Lua: return "Lua Script";
    case FileKind::Car: return "Car Config";
    case FileKind::Ifl: return "IFL Frame List";
    case FileKind::Col: return "Collision";
    default: return "Unknown";
    }
}

uint32_t kind_colour(FileKind kind) {
    if (kind_is_image(kind)) return kColourImage;
    if (kind_is_model(kind)) return kColourModel;
    if (kind_is_audio(kind)) return kColourAudio;
    if (kind == FileKind::Car) return kColourCar;
    if (kind_is_text(kind)) return kColourText;
    return kColourOther;
}

bool kind_is_image(FileKind kind) {
    return kind == FileKind::Dds || kind == FileKind::Png || kind == FileKind::Bmp ||
           kind == FileKind::Jpg || kind == FileKind::Tga;
}

bool kind_is_model(FileKind kind) {
    return kind == FileKind::Nif || kind == FileKind::Kf || kind == FileKind::Kfm;
}

bool kind_is_audio(FileKind kind) {
    return kind == FileKind::Wav || kind == FileKind::Ogg || kind == FileKind::Mp3;
}

bool kind_is_text(FileKind kind) {
    return kind == FileKind::Xml || kind == FileKind::Txt || kind == FileKind::Ini ||
           kind == FileKind::Csv || kind == FileKind::Lua || kind == FileKind::Car ||
           kind == FileKind::Ifl;
}

std::string lower_text(std::string text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

std::string format_size(uint64_t bytes) {
    char buffer[64];
    if (bytes >= 1024u * 1024u)
        std::snprintf(buffer, sizeof(buffer), "%.1f MB (%llu B)", bytes / (1024.0 * 1024.0),
                      static_cast<unsigned long long>(bytes));
    else if (bytes >= 1024u)
        std::snprintf(buffer, sizeof(buffer), "%.1f KB (%llu B)", bytes / 1024.0,
                      static_cast<unsigned long long>(bytes));
    else
        std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
    return buffer;
}

Archive::~Archive() { clear_cache(); }

std::string Archive::clean_path(const std::string& path) {
    std::string clean = path;
    for (char& c : clean) if (c == '\\') c = '/';
    while (clean.rfind("./", 0) == 0) clean.erase(0, 2);
    return clean;
}

std::string Archive::folder_of(const std::string& clean) {
    const size_t slash = clean.rfind('/');
    return slash == std::string::npos ? std::string() : clean.substr(0, slash);
}

std::string Archive::name_of(const std::string& clean) {
    const size_t slash = clean.rfind('/');
    return slash == std::string::npos ? clean : clean.substr(slash + 1);
}

std::string Archive::stem_of(const std::string& name) {
    const size_t dot = name.rfind('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

bool Archive::open(const std::string& game_dir) {
    clear_cache();
    by_path_.clear();
    by_name_.clear();
    game_dir_ = game_dir;
    if (!reader_.OpenGameDir(game_dir)) {
        reader_.Close();
        return false;
    }
    const auto& list = reader_.GetEntries();
    for (int index = 0; index < static_cast<int>(list.size()); ++index) {
        const std::string clean = lower_text(clean_path(list[index].path));
        by_path_.emplace(clean, index);
        by_name_[lower_text(name_of(clean))].push_back(index);
    }
    // One cache folder per process so two instances never wipe each other
    std::error_code ignored;
    cache_root_ = fs::temp_directory_path(ignored) / ("knc_dat_manager_" + std::to_string(process_id()));
    fs::remove_all(cache_root_, ignored);
    return true;
}

std::vector<uint8_t> Archive::read(int index) {
    if (index < 0 || index >= static_cast<int>(entries().size())) return {};
    return reader_.ReadPath(entries()[index].path);
}

int Archive::find_path(const std::string& path) const {
    const auto found = by_path_.find(lower_text(clean_path(path)));
    return found == by_path_.end() ? -1 : found->second;
}

int Archive::find_named_near(const std::string& folder, const std::string& name) const {
    const auto found = by_name_.find(lower_text(name));
    if (found == by_name_.end()) return -1;
    const std::string want = lower_text(clean_path(folder));
    const std::string parent = folder_of(want);
    int best = -1;
    int best_score = -1;
    for (int index : found->second) {
        const std::string dir = lower_text(folder_of(clean_path(entries()[index].path)));
        int score = 1;
        if (dir == want) score = 4;
        else if (dir.rfind(want + "/", 0) == 0) score = 3;
        else if (!parent.empty() && dir.rfind(parent + "/", 0) == 0) score = 2;
        score = score * 2 + (dir.find("high") != std::string::npos ? 1 : 0);
        if (score > best_score) {
            best_score = score;
            best = index;
        }
    }
    return best;
}

std::vector<int> Archive::entries_in_folder(const std::string& folder) const {
    const std::string want = lower_text(clean_path(folder));
    std::vector<int> out;
    const auto& list = entries();
    for (int index = 0; index < static_cast<int>(list.size()); ++index)
        if (lower_text(folder_of(clean_path(list[index].path))) == want) out.push_back(index);
    return out;
}

std::string Archive::cache_path(int index) {
    const auto known = cached_.find(index);
    if (known != cached_.end()) return known->second;
    if (index < 0 || index >= static_cast<int>(entries().size())) return std::string();
    const fs::path target = cache_root_ / clean_path(entries()[index].path);
    const std::vector<uint8_t> bytes = read(index);
    if (bytes.empty() || !write_bytes(target, bytes)) {
        std::printf("[cache] cannot write %s\n", target.string().c_str());
        return std::string();
    }
    const std::string path = target.string();
    cached_[index] = path;
    return path;
}

void Archive::clear_cache() {
    cached_.clear();
    if (cache_root_.empty()) return;
    std::error_code ignored;
    fs::remove_all(cache_root_, ignored);
}

bool Listing::passes(const KnC::PakEntry& entry) const {
    const FileKind kind = file_kind(entry.filename);
    if (filter == FilterKind::Image && !kind_is_image(kind)) return false;
    if (filter == FilterKind::Model && !kind_is_model(kind)) return false;
    if (filter == FilterKind::Audio && !kind_is_audio(kind)) return false;
    if (search.empty()) return true;
    return lower_text(entry.path).find(search) != std::string::npos;
}

void Listing::rebuild(const Archive& archive) {
    flat.clear();
    root = FolderNode();
    root.name = "Root";
    const auto& list = archive.entries();
    for (int index = 0; index < static_cast<int>(list.size()); ++index) {
        if (!passes(list[index])) continue;
        flat.push_back(index);
        const std::string clean = Archive::clean_path(list[index].path);
        FolderNode* node = &root;
        ++node->total_files;
        size_t start = 0;
        while (true) {
            const size_t slash = clean.find('/', start);
            if (slash == std::string::npos) break;
            const std::string part = clean.substr(start, slash - start);
            FolderNode& child = node->folders[part];
            if (child.name.empty()) {
                child.name = part;
                child.path = clean.substr(0, slash);
            }
            node = &child;
            ++node->total_files;
            start = slash + 1;
        }
        node->files.push_back(index);
    }
}

bool extract_entry(Archive& archive, int index, const fs::path& out_dir, std::string& error) {
    const std::vector<uint8_t> bytes = archive.read(index);
    if (bytes.empty()) {
        error = "cannot read the entry";
        return false;
    }
    const fs::path target = out_dir / archive.entries()[index].filename;
    if (!write_bytes(target, bytes)) {
        error = "cannot write " + target.string();
        return false;
    }
    return true;
}

namespace {

int extract_tree(Archive& archive, const FolderNode& folder, const fs::path& out_dir, size_t trim) {
    int count = 0;
    for (int index : folder.files) {
        const std::vector<uint8_t> bytes = archive.read(index);
        if (bytes.empty()) continue;
        const std::string clean = Archive::clean_path(archive.entries()[index].path);
        if (write_bytes(out_dir / clean.substr(trim), bytes)) ++count;
    }
    for (const auto& child : folder.folders) count += extract_tree(archive, child.second, out_dir, trim);
    return count;
}

}

int extract_folder(Archive& archive, const FolderNode& folder, const fs::path& out_dir) {
    // The folder lands under its own name so the parent part of the path is trimmed
    const std::string parent = Archive::folder_of(folder.path);
    const size_t trim = parent.empty() ? 0 : parent.size() + 1;
    return extract_tree(archive, folder, out_dir, trim);
}

std::string load_game_dir_setting() {
    std::ifstream in(kSettingsFile);
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.rfind("GameDir=", 0) == 0) return line.substr(8);
    }
    return std::string();
}

void save_game_dir_setting(const std::string& game_dir) {
    std::ofstream out(kSettingsFile);
    if (out) out << "[Settings]\nGameDir=" << game_dir << "\n";
}

}
