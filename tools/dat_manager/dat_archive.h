// The pak archive of one game folder its file kinds the filtered listing and the extract
#pragma once

#include "PakReader.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace KnC::Tools {

enum class FileKind {
    Unknown, Dds, Png, Bmp, Jpg, Tga, Nif, Kf, Kfm, Wav, Ogg, Mp3,
    Xml, Txt, Ini, Csv, Lua, Car, Ifl, Col, Other
};

enum class FilterKind { All, Image, Model, Audio };

FileKind    file_kind(const std::string& name);
const char* kind_name(FileKind kind);
// Packed ABGR colour the list paints the kind with
uint32_t    kind_colour(FileKind kind);
bool        kind_is_image(FileKind kind);
bool        kind_is_model(FileKind kind);
bool        kind_is_audio(FileKind kind);
bool        kind_is_text(FileKind kind);

std::string lower_text(std::string text);
std::string format_size(uint64_t bytes);

// One folder of the tree view files hold entry indices
struct FolderNode {
    std::string name;
    std::string path;
    std::map<std::string, FolderNode> folders;
    std::vector<int> files;
    int total_files = 0;
};

// The pak files of a game folder through PakReader plus a disk cache for the loaders
class Archive {
public:
    ~Archive();

    bool open(const std::string& game_dir);
    bool opened() const { return reader_.GetEntryCount() != 0; }
    const std::string& game_dir() const { return game_dir_; }
    const std::vector<KnC::PakEntry>& entries() const { return reader_.GetEntries(); }

    // The pak path without its leading dot slash
    static std::string clean_path(const std::string& path);
    static std::string folder_of(const std::string& clean);
    static std::string name_of(const std::string& clean);
    static std::string stem_of(const std::string& name);

    std::vector<uint8_t> read(int index);
    // Entry index of one path minus one when the pak holds none
    int find_path(const std::string& path) const;
    // Entry of that name the same folder first then below it high before low
    int find_named_near(const std::string& folder, const std::string& name) const;
    // Every entry inside one folder no subfolder
    std::vector<int> entries_in_folder(const std::string& folder) const;

    // Writes one entry into the cache folder once and returns its disk path
    std::string cache_path(int index);
    void clear_cache();
    const std::filesystem::path& cache_root() const { return cache_root_; }

private:
    KnC::PakReader reader_;
    std::string game_dir_;
    std::filesystem::path cache_root_;
    std::map<std::string, int> by_path_;
    std::map<std::string, std::vector<int>> by_name_;
    std::map<int, std::string> cached_;
};

// The flat list and the folder tree of the entries passing the filter and the search
struct Listing {
    FilterKind  filter = FilterKind::All;
    std::string search;
    std::vector<int> flat;
    FolderNode  root;

    void rebuild(const Archive& archive);
    bool passes(const KnC::PakEntry& entry) const;
};

// Writes the entry under its own name inside the folder
bool extract_entry(Archive& archive, int index, const std::filesystem::path& out_dir, std::string& error);
// Writes the folder tree inside the out folder returns the file count
int extract_folder(Archive& archive, const FolderNode& folder, const std::filesystem::path& out_dir);
// Packs a folder into one pak file with the pak tool code
bool pack_folder(const std::string& out_pak, const std::string& folder, std::string& error);

// The game folder kept in dat manager ini beside the exe
std::string load_game_dir_setting();
void        save_game_dir_setting(const std::string& game_dir);

}
