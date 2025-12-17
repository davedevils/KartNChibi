/**
 * KnC DAT Manager v2
 * ------------------
 * GUI tool to browse and extract PAK files
 * 
 * Features:
 * - Two view modes: List / Tree (folders)
 * - Filter by type (All, DDS, NIF, Audio)
 * - Preview DDS textures
 * - Preview NIF models (real 3D)
 * - Preview/play audio (WAV, OGG)
 * - Extract files
 */

#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <direct.h>

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "../../engine/bridge/NifBridge.h"
#include "../../common/PakReader.h"

using namespace KnC;

//=============================================================================
// GLOBALS
//=============================================================================
PakReader g_pak;
std::vector<size_t> g_filteredIndices;
std::string g_searchQuery;
int g_filterType = 0;  // 0=All, 1=DDS, 2=NIF, 3=Audio
int g_selectedEntry = -1;
int g_scrollOffset = 0;
bool g_viewModeTree = true;  // Default to tree view

// Tree view - simple approach
struct FolderNode {
    std::string name;
    std::string fullPath;
    bool expanded;
    std::map<std::string, FolderNode> subfolders;
    std::vector<int> fileIndices;  // indices into pak entries
};
FolderNode g_rootFolder;

// Flat list for rendering
struct TreeItem {
    std::string name;
    std::string fullPath;
    int depth;
    bool isFolder;
    bool expanded;
    int entryIndex;  // -1 for folders
    FolderNode* folderPtr;  // for folders
};
std::vector<TreeItem> g_treeItems;
int g_treeScroll = 0;

// Preview - Texture
Texture2D g_previewTexture = {0};
bool g_hasTexturePreview = false;

// Preview - 3D Model  
std::vector<Model> g_previewModels;
RenderTexture2D g_modelRenderTex = {0};
bool g_hasModelPreview = false;
float g_modelRotY = 0;
float g_modelZoom = 5.0f;
Camera3D g_previewCam = {0};
BoundingBox g_modelBounds;

// Preview - Audio
Sound g_previewSound = {0};
Music g_previewMusic = {0};
bool g_hasSound = false;
bool g_hasMusic = false;
bool g_audioPlaying = false;
float g_audioDuration = 0;

std::string g_previewInfo;
std::string g_gameDir;
char g_searchBuffer[256] = {0};

// Folder browser
bool g_showBrowser = false;
std::string g_browserPath;
std::vector<std::string> g_browserDirs;
int g_browserScroll = 0;

// Scrollbar drag state
bool g_draggingScrollbar = false;
float g_scrollDragStartY = 0;
int g_scrollDragStartVal = 0;

// Colors
Color COL_BG = {30, 30, 35, 255};
Color COL_PANEL = {40, 42, 48, 255};
Color COL_ACCENT = {80, 140, 200, 255};
Color COL_SELECTED = {60, 100, 160, 255};
Color COL_HOVER = {50, 55, 65, 255};
Color COL_TEXT = {220, 220, 225, 255};
Color COL_TEXT_DIM = {140, 140, 150, 255};
Color COL_DDS = {100, 180, 100, 255};
Color COL_NIF = {180, 140, 100, 255};
Color COL_WAV = {100, 140, 200, 255};
Color COL_FOLDER = {220, 190, 90, 255};

// Color key
const unsigned char COLORKEY_R = 0, COLORKEY_G = 0, COLORKEY_B = 255;
const int COLORKEY_THRESHOLD = 10;

//=============================================================================
// CONFIG
//=============================================================================
void SaveConfig() {
    std::ofstream f("dat_manager.ini");
    if (f.is_open()) { f << "[Settings]\nGameDir=" << g_gameDir << "\n"; f.close(); }
}

void LoadConfig() {
    std::ifstream f("dat_manager.ini");
    if (f.is_open()) {
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("GameDir=") == 0) g_gameDir = line.substr(8);
        }
        f.close();
    }
}

//=============================================================================
// FILE TYPE
//=============================================================================
enum FileType { 
    TYPE_UNKNOWN, 
    TYPE_DDS, TYPE_PNG, TYPE_BMP, TYPE_JPG, TYPE_TGA,  // Images
    TYPE_NIF, TYPE_KF,                                  // Models & Animations
    TYPE_WAV, TYPE_OGG, TYPE_MP3,                      // Audio
    TYPE_BNK, TYPE_FSB,                                 // Sound banks
    TYPE_ZIP, TYPE_RAR,                                 // Archives
    TYPE_XML, TYPE_TXT, TYPE_INI, TYPE_CSV, TYPE_LUA,  // Config files
    TYPE_CAR,                                           // Car config
    TYPE_OTHER
};

FileType GetFileType(const std::string& filename) {
    size_t dot = filename.rfind('.');
    if (dot == std::string::npos) return TYPE_UNKNOWN;
    
    std::string ext = filename.substr(dot);
    for (char& c : ext) c = tolower(c);
    
    // Images
    if (ext == ".dds") return TYPE_DDS;
    if (ext == ".png") return TYPE_PNG;
    if (ext == ".bmp") return TYPE_BMP;
    if (ext == ".jpg" || ext == ".jpeg") return TYPE_JPG;
    if (ext == ".tga") return TYPE_TGA;
    
    // Models
    if (ext == ".nif") return TYPE_NIF;
    if (ext == ".kf" || ext == ".kfm") return TYPE_KF;
    
    // Audio
    if (ext == ".wav") return TYPE_WAV;
    if (ext == ".ogg") return TYPE_OGG;
    if (ext == ".mp3") return TYPE_MP3;
    if (ext == ".bnk") return TYPE_BNK;
    if (ext == ".fsb") return TYPE_FSB;
    
    // Archives
    if (ext == ".zip") return TYPE_ZIP;
    if (ext == ".rar") return TYPE_RAR;
    
    // Config
    if (ext == ".xml") return TYPE_XML;
    if (ext == ".txt") return TYPE_TXT;
    if (ext == ".ini") return TYPE_INI;
    if (ext == ".csv") return TYPE_CSV;
    if (ext == ".lua") return TYPE_LUA;
    
    // Car config
    if (ext == ".car") return TYPE_CAR;
    
    return TYPE_OTHER;
}

Color GetFileColor(FileType type) {
    switch (type) {
        case TYPE_DDS: 
        case TYPE_PNG:
        case TYPE_BMP:
        case TYPE_JPG:
        case TYPE_TGA:
            return COL_DDS;  // Green for images
        case TYPE_NIF:
        case TYPE_KF:
            return COL_NIF;  // Orange for models
        case TYPE_WAV: 
        case TYPE_OGG: 
        case TYPE_MP3:
        case TYPE_BNK:
        case TYPE_FSB:
            return COL_WAV;  // Blue for audio
        case TYPE_ZIP:
        case TYPE_RAR:
            return {200, 100, 200, 255};  // Purple for archives
        case TYPE_XML:
        case TYPE_TXT:
        case TYPE_INI:
        case TYPE_CSV:
        case TYPE_LUA:
            return {180, 180, 180, 255};  // Gray for config
        case TYPE_CAR:
            return {255, 180, 80, 255};  // Yellow-orange for car config
        default: 
            return COL_TEXT_DIM;
    }
}

bool PassesFilter(int entryIndex) {
    const auto& entries = g_pak.GetEntries();
    const auto& e = entries[entryIndex];
    
    FileType ft = GetFileType(e.filename);
    
    // Filter by type: 0=All, 1=Images, 2=Models, 3=Audio
    if (g_filterType == 1) {
        if (ft != TYPE_DDS && ft != TYPE_PNG && ft != TYPE_BMP && ft != TYPE_JPG && ft != TYPE_TGA) 
            return false;
    }
    if (g_filterType == 2) {
        if (ft != TYPE_NIF && ft != TYPE_KF) return false;
    }
    if (g_filterType == 3) {
        if (ft != TYPE_WAV && ft != TYPE_OGG && ft != TYPE_MP3 && ft != TYPE_BNK && ft != TYPE_FSB) 
            return false;
    }
    
    if (!g_searchQuery.empty()) {
        std::string lower = e.path;
        for (char& c : lower) c = tolower(c);
        if (lower.find(g_searchQuery) == std::string::npos) return false;
    }
    
    return true;
}

//=============================================================================
// TEXTURE LOADING FROM PAK
//=============================================================================
std::map<std::string, Texture2D> g_texCache;

void ApplyColorKey(Image* img) {
    if (!img->data || img->format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) return;
    unsigned char* pixels = (unsigned char*)img->data;
    for (int i = 0; i < img->width * img->height; i++) {
        int r = pixels[i * 4 + 0];
        int g = pixels[i * 4 + 1];
        int b = pixels[i * 4 + 2];
        if (abs(r - COLORKEY_R) < COLORKEY_THRESHOLD &&
            abs(g - COLORKEY_G) < COLORKEY_THRESHOLD &&
            abs(b - COLORKEY_B) < COLORKEY_THRESHOLD) {
            pixels[i * 4 + 3] = 0;
        }
    }
}

Texture2D LoadTextureFromPak(const std::string& texPath) {
    std::string lower = texPath;
    for (char& c : lower) { c = tolower(c); if (c == '\\') c = '/'; }
    
    auto it = g_texCache.find(lower);
    if (it != g_texCache.end()) return it->second;
    
    std::string filename = texPath;
    size_t slash = filename.rfind('/');
    if (slash == std::string::npos) slash = filename.rfind('\\');
    if (slash != std::string::npos) filename = filename.substr(slash + 1);
    
    // Get base name without extension
    std::string baseName = filename;
    size_t dot = baseName.rfind('.');
    if (dot != std::string::npos) baseName = baseName.substr(0, dot);
    
    // Try multiple formats
    const char* exts[] = {".dds", ".tga", ".png", ".bmp", ".jpg"};
    std::vector<uint8_t> data;
    const char* loadedExt = ".dds";
    
    // First try original filename
    data = g_pak.ReadFile(filename);
    if (!data.empty()) {
        if (filename.find(".tga") != std::string::npos) loadedExt = ".tga";
        else if (filename.find(".png") != std::string::npos) loadedExt = ".png";
        else if (filename.find(".bmp") != std::string::npos) loadedExt = ".bmp";
        else if (filename.find(".jpg") != std::string::npos) loadedExt = ".jpg";
    }
    
    // If not found, try alternatives
    if (data.empty()) {
        for (const char* ext : exts) {
            std::string altName = baseName + ext;
            data = g_pak.ReadFile(altName);
            if (!data.empty()) {
                loadedExt = ext;
                break;
            }
        }
    }
    
    if (data.empty()) return Texture2D{0};
    
    Image img = LoadImageFromMemory(loadedExt, data.data(), (int)data.size());
    if (!img.data) return Texture2D{0};
    
    // Only convert uncompressed formats, keep DXT compressed as-is
    bool isCompressed = (img.format >= 13 && img.format <= 23);  // DXT/ETC/ASTC
    
    if (!isCompressed) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
        ApplyColorKey(&img);
    }
    
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    
    g_texCache[lower] = tex;
    return tex;
}

//=============================================================================
// TREE VIEW
//=============================================================================
void BuildFolderTree() {
    g_rootFolder = FolderNode();
    g_rootFolder.name = "Root";
    g_rootFolder.expanded = true;
    
    const auto& entries = g_pak.GetEntries();
    
    for (size_t i = 0; i < entries.size(); i++) {
        if (!PassesFilter((int)i)) continue;
        
        const auto& e = entries[i];
        std::string path = e.path;
        for (char& c : path) if (c == '\\') c = '/';
        
        // Split into parts
        std::vector<std::string> parts;
        size_t start = 0;
        for (size_t j = 0; j <= path.size(); j++) {
            if (j == path.size() || path[j] == '/') {
                if (j > start) parts.push_back(path.substr(start, j - start));
                start = j + 1;
            }
        }
        
        if (parts.empty()) continue;
        
        // Navigate to correct folder
        FolderNode* current = &g_rootFolder;
        std::string currentPath;
        
        for (size_t j = 0; j < parts.size() - 1; j++) {
            if (!currentPath.empty()) currentPath += "/";
            currentPath += parts[j];
            
            auto& sub = current->subfolders[parts[j]];
            if (sub.name.empty()) {
                sub.name = parts[j];
                sub.fullPath = currentPath;
                sub.expanded = false;
            }
            current = &sub;
        }
        
        // Add file to folder
        current->fileIndices.push_back((int)i);
    }
}

void FlattenFolder(FolderNode* folder, int depth) {
    // Sort and add subfolders
    std::vector<std::string> folderNames;
    for (auto& pair : folder->subfolders) {
        folderNames.push_back(pair.first);
    }
    std::sort(folderNames.begin(), folderNames.end());
    
    for (const auto& name : folderNames) {
        FolderNode* sub = &folder->subfolders[name];
        
        TreeItem item;
        item.name = sub->name;
        item.fullPath = sub->fullPath;
        item.depth = depth;
        item.isFolder = true;
        item.expanded = sub->expanded;
        item.entryIndex = -1;
        item.folderPtr = sub;
        g_treeItems.push_back(item);
        
        if (sub->expanded) {
            FlattenFolder(sub, depth + 1);
        }
    }
    
    // Sort and add files
    const auto& entries = g_pak.GetEntries();
    std::vector<int> sortedFiles = folder->fileIndices;
    std::sort(sortedFiles.begin(), sortedFiles.end(), [&entries](int a, int b) {
        return entries[a].filename < entries[b].filename;
    });
    
    for (int idx : sortedFiles) {
        TreeItem item;
        item.name = entries[idx].filename;
        item.fullPath = entries[idx].path;
        item.depth = depth;
        item.isFolder = false;
        item.expanded = false;
        item.entryIndex = idx;
        item.folderPtr = nullptr;
        g_treeItems.push_back(item);
    }
}

void RebuildTreeItems() {
    g_treeItems.clear();
    FlattenFolder(&g_rootFolder, 0);
}

//=============================================================================
// FOLDER BROWSER
//=============================================================================
void UpdateBrowserDirs() {
    g_browserDirs.clear();
    g_browserScroll = 0;
    
    if (g_browserPath.empty()) {
        for (char c = 'A'; c <= 'Z'; c++) {
            char drive[4] = {c, ':', '/', 0};
            if (DirectoryExists(drive)) g_browserDirs.push_back(std::string(1, c) + ":/");
        }
        return;
    }
    
    g_browserDirs.push_back("..");
    FilePathList list = LoadDirectoryFiles(g_browserPath.c_str());
    for (unsigned int i = 0; i < list.count; i++) {
        if (DirectoryExists(list.paths[i])) {
            const char* name = GetFileName(list.paths[i]);
            if (name[0] != '.') g_browserDirs.push_back(name);
        }
    }
    UnloadDirectoryFiles(list);
    std::sort(g_browserDirs.begin() + 1, g_browserDirs.end());
}

//=============================================================================
// FILTERING
//=============================================================================
void UpdateFilter() {
    g_filteredIndices.clear();
    const auto& entries = g_pak.GetEntries();
    
    for (size_t i = 0; i < entries.size(); i++) {
        if (PassesFilter((int)i)) {
            g_filteredIndices.push_back(i);
        }
    }
    
    g_selectedEntry = -1;
    g_scrollOffset = 0;
    
    BuildFolderTree();
    RebuildTreeItems();
    g_treeScroll = 0;
}

//=============================================================================
// PREVIEW
//=============================================================================
void ClearPreview() {
    if (g_hasTexturePreview && g_previewTexture.id > 0) UnloadTexture(g_previewTexture);
    g_previewTexture = {0};
    g_hasTexturePreview = false;
    
    for (auto& m : g_previewModels) {
        if (m.meshCount > 0) UnloadModel(m);
    }
    g_previewModels.clear();
    g_hasModelPreview = false;
    
    if (g_hasSound) { StopSound(g_previewSound); UnloadSound(g_previewSound); }
    g_hasSound = false;
    
    if (g_hasMusic) { StopMusicStream(g_previewMusic); UnloadMusicStream(g_previewMusic); }
    g_hasMusic = false;
    
    g_audioPlaying = false;
    g_previewInfo.clear();
}

void LoadNifPreview(const std::vector<uint8_t>& data, const std::string& filename) {
    // Save to temp file for NifBridge
    std::string tempPath = "temp_nif_preview.nif";
    {
        std::ofstream f(tempPath, std::ios::binary);
        if (!f.is_open()) {
            g_previewInfo += "\n[Error: Cannot create temp file]";
            return;
        }
        f.write((char*)data.data(), data.size());
        f.close();
    }
    
    // Load using our NifBridge
    printf("[Preview] Loading NIF: %s (%d bytes)\n", filename.c_str(), (int)data.size());
    NifResult result = LoadNif(tempPath, false);
    remove(tempPath.c_str());
    
    if (result.meshes.empty()) {
        g_previewInfo += "\n[No geometry found in NIF]";
        printf("[Preview] No meshes found\n");
        return;
    }
    
    char buf[128];
    sprintf(buf, "\nMeshes: %d", (int)result.meshes.size());
    g_previewInfo += buf;
    printf("[Preview] Found %d meshes\n", (int)result.meshes.size());
    
    // Reset bounds
    g_modelBounds.min = {999999, 999999, 999999};
    g_modelBounds.max = {-999999, -999999, -999999};
    
    int totalVerts = 0;
    for (const auto& meshData : result.meshes) {
        if (meshData.vertices.empty() || meshData.indices.empty()) continue;
        
        int vertCount = (int)meshData.vertices.size() / 3;
        int triCount = (int)meshData.indices.size() / 3;
        
        if (triCount == 0 || vertCount == 0) continue;
        
        // Use the helper function from NifBridge
        Mesh mesh = ToRaylibMesh(meshData);
        
        if (mesh.vertexCount == 0) continue;
        
        // Update bounds
        for (int i = 0; i < mesh.vertexCount; i++) {
            Vector3 pos = {mesh.vertices[i*3], mesh.vertices[i*3+1], mesh.vertices[i*3+2]};
            if (pos.x < g_modelBounds.min.x) g_modelBounds.min.x = pos.x;
            if (pos.y < g_modelBounds.min.y) g_modelBounds.min.y = pos.y;
            if (pos.z < g_modelBounds.min.z) g_modelBounds.min.z = pos.z;
            if (pos.x > g_modelBounds.max.x) g_modelBounds.max.x = pos.x;
            if (pos.y > g_modelBounds.max.y) g_modelBounds.max.y = pos.y;
            if (pos.z > g_modelBounds.max.z) g_modelBounds.max.z = pos.z;
        }
        
        Model model = LoadModelFromMesh(mesh);
        
        // Try to load texture
        if (!meshData.texturePath.empty()) {
            Texture2D tex = LoadTextureFromPak(meshData.texturePath);
            if (tex.id > 0) {
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;
            }
        }
        
        g_previewModels.push_back(model);
        totalVerts += mesh.vertexCount;
    }
    
    sprintf(buf, "\nVertices: %d", totalVerts);
    g_previewInfo += buf;
    
    // Calculate zoom
    Vector3 size = {
        g_modelBounds.max.x - g_modelBounds.min.x,
        g_modelBounds.max.y - g_modelBounds.min.y,
        g_modelBounds.max.z - g_modelBounds.min.z
    };
    float maxDim = fmaxf(fmaxf(size.x, size.y), size.z);
    g_modelZoom = maxDim * 1.5f;
    if (g_modelZoom < 2) g_modelZoom = 2;
    if (g_modelZoom > 500) g_modelZoom = 500;
    
    g_hasModelPreview = !g_previewModels.empty();
    g_modelRotY = 0;
    
    printf("[Preview] Loaded %d models, zoom=%.1f\n", (int)g_previewModels.size(), g_modelZoom);
}

void LoadPreview(const PakEntry& entry) {
    ClearPreview();
    
    FileType ft = GetFileType(entry.filename);
    
    char buf[512];
    g_previewInfo = "File: " + entry.filename + "\n";
    g_previewInfo += "Path: " + entry.path + "\n";
    sprintf(buf, "Size: %d KB\n", entry.size / 1024);
    g_previewInfo += buf;
    sprintf(buf, "PAK: pak%03d.dat", entry.pakIndex + 1);
    g_previewInfo += buf;
    
    std::vector<uint8_t> data = g_pak.ReadPath(entry.path);
    if (data.empty()) {
        g_previewInfo += "\n[Error: Could not read file]";
        return;
    }
    
    if (ft == TYPE_DDS || ft == TYPE_PNG || ft == TYPE_BMP || ft == TYPE_JPG || ft == TYPE_TGA) {
        const char* ext = ".dds";
        if (ft == TYPE_PNG) ext = ".png";
        else if (ft == TYPE_BMP) ext = ".bmp";
        else if (ft == TYPE_JPG) ext = ".jpg";
        else if (ft == TYPE_TGA) ext = ".tga";
        
        Image img = LoadImageFromMemory(ext, data.data(), (int)data.size());
        if (img.data) {
            // Show format info
            const char* fmtNames[] = {"GRAYSCALE", "GRAY_ALPHA", "R5G6B5", "R8G8B8", "R5G5B5A1", 
                "R4G4B4A4", "R8G8B8A8", "R32", "R32G32B32", "R32G32B32A32", "R16", "R16G16B16", 
                "R16G16B16A16", "DXT1_RGB", "DXT1_RGBA", "DXT3_RGBA", "DXT5_RGBA", "ETC1_RGB",
                "ETC2_RGB", "ETC2_EAC_RGBA", "PVRT_RGB", "PVRT_RGBA", "ASTC_4x4", "ASTC_8x8"};
            const char* fmtName = (img.format >= 0 && img.format < 24) ? fmtNames[img.format] : "UNKNOWN";
            sprintf(buf, "\n\nDimensions: %dx%d\nFormat: %s", img.width, img.height, fmtName);
            g_previewInfo += buf;
            
            // For preview, try to convert to displayable format if compressed
            bool isCompressed = (img.format >= 13 && img.format <= 23);
            
            // Resize for preview
            if (!isCompressed && (img.width > 400 || img.height > 400)) {
                float scale = 400.0f / (float)((img.width > img.height) ? img.width : img.height);
                ImageResize(&img, (int)(img.width * scale), (int)(img.height * scale));
            }
            
            g_previewTexture = LoadTextureFromImage(img);
            g_hasTexturePreview = (g_previewTexture.id > 0);
            UnloadImage(img);
        } else {
            g_previewInfo += "\n\n[Cannot decode image]";
        }
    }
    else if (ft == TYPE_NIF) {
        LoadNifPreview(data, entry.filename);
    }
    else if (ft == TYPE_WAV) {
        std::string tempPath = "temp_audio.wav";
        std::ofstream f(tempPath, std::ios::binary);
        if (f.is_open()) {
            f.write((char*)data.data(), data.size());
            f.close();
            
            Wave wave = LoadWave(tempPath.c_str());
            if (wave.data) {
                g_previewSound = LoadSoundFromWave(wave);
                g_hasSound = true;
                g_audioDuration = (float)wave.frameCount / wave.sampleRate;
                sprintf(buf, "\n\nSample Rate: %d Hz\nDuration: %.2fs", wave.sampleRate, g_audioDuration);
                g_previewInfo += buf;
                UnloadWave(wave);
            }
            remove(tempPath.c_str());
        }
    }
    else if (ft == TYPE_OGG || ft == TYPE_MP3) {
        std::string tempPath = (ft == TYPE_OGG) ? "temp_audio.ogg" : "temp_audio.mp3";
        std::ofstream f(tempPath, std::ios::binary);
        if (f.is_open()) {
            f.write((char*)data.data(), data.size());
            f.close();
            
            g_previewMusic = LoadMusicStream(tempPath.c_str());
            if (g_previewMusic.stream.buffer) {
                g_hasMusic = true;
                g_audioDuration = GetMusicTimeLength(g_previewMusic);
                sprintf(buf, "\n\nDuration: %.2fs", g_audioDuration);
                g_previewInfo += buf;
            } else {
                g_previewInfo += "\n\n[Cannot decode audio]";
            }
        }
    }
    else if (ft == TYPE_CAR || ft == TYPE_TXT || ft == TYPE_XML || ft == TYPE_INI || 
             ft == TYPE_CSV || ft == TYPE_LUA) {
        // Text file preview
        g_previewInfo += "\n\n--- Content ---\n";
        
        // Limit preview to first 2KB
        size_t previewLen = (data.size() > 2048) ? 2048 : data.size();
        std::string content((char*)data.data(), previewLen);
        
        // Clean up for display
        for (char& c : content) {
            if (c == '\r') c = ' ';
            if (c < 32 && c != '\n' && c != '\t') c = '.';
        }
        
        g_previewInfo += content;
        if (data.size() > 2048) {
            g_previewInfo += "\n\n... [truncated]";
        }
    }
}

//=============================================================================
// EXTRACT
//=============================================================================
void CreateDirs(const std::string& path) {
    std::string dir;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '/' || path[i] == '\\') {
            dir = path.substr(0, i);
            if (!dir.empty()) _mkdir(dir.c_str());
        }
    }
}

void ExtractFile(const PakEntry& entry) {
    std::vector<uint8_t> data = g_pak.ReadPath(entry.path);
    if (data.empty()) return;
    
    std::string outPath = "extracted/" + entry.path;
    for (char& c : outPath) if (c == '\\') c = '/';
    CreateDirs(outPath);
    
    std::ofstream f(outPath, std::ios::binary);
    if (f.is_open()) {
        f.write((char*)data.data(), data.size());
        f.close();
        g_previewInfo += "\n\n[Extracted!]";
    }
}

//=============================================================================
// DRAW - Folder Browser
//=============================================================================
void DrawFolderBrowser() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    DrawRectangle(0, 0, W, H, Fade(BLACK, 0.7f));
    
    int bw = 500, bh = 450;
    int bx = (W - bw) / 2, by = (H - bh) / 2;
    
    DrawRectangle(bx, by, bw, bh, COL_PANEL);
    DrawRectangleLines(bx, by, bw, bh, COL_ACCENT);
    DrawText("Select Game Directory", bx + 20, by + 15, 20, COL_TEXT);
    
    DrawRectangle(bx + 20, by + 50, bw - 40, 25, COL_HOVER);
    DrawText(g_browserPath.empty() ? "Select Drive..." : g_browserPath.c_str(), bx + 25, by + 55, 14, COL_TEXT);
    
    int listY = by + 85, listH = bh - 155, itemH = 24;
    int visibleItems = listH / itemH;
    int listW = bw - 60;
    
    DrawRectangle(bx + 20, listY, listW, listH, COL_BG);
    
    float wheel = GetMouseWheelMove();
    Rectangle listRect = {(float)bx + 20, (float)listY, (float)listW, (float)listH};
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), listRect)) {
        g_browserScroll -= (int)(wheel * 3);
        if (g_browserScroll < 0) g_browserScroll = 0;
        int maxScroll = (int)g_browserDirs.size() - visibleItems;
        if (maxScroll < 0) maxScroll = 0;
        if (g_browserScroll > maxScroll) g_browserScroll = maxScroll;
    }
    
    BeginScissorMode(bx + 20, listY, listW, listH);
    for (int i = 0; i < visibleItems && (g_browserScroll + i) < (int)g_browserDirs.size(); i++) {
        const std::string& dir = g_browserDirs[g_browserScroll + i];
        Rectangle row = {(float)bx + 20, (float)(listY + i * itemH), (float)listW, (float)itemH};
        
        bool hover = CheckCollisionPointRec(GetMousePosition(), row);
        if (hover) DrawRectangleRec(row, COL_HOVER);
        
        DrawText(">", (int)row.x + 8, (int)row.y + 4, 14, COL_FOLDER);
        DrawText(dir.c_str(), (int)row.x + 25, (int)row.y + 4, 15, COL_TEXT);
        
        if (hover && IsMouseButtonPressed(0)) {
            if (dir == "..") {
                size_t slash = g_browserPath.rfind('/');
                if (slash != std::string::npos && slash > 2) g_browserPath = g_browserPath.substr(0, slash);
                else if (g_browserPath.size() > 3) g_browserPath = g_browserPath.substr(0, 3);
                else g_browserPath = "";
            } else if (g_browserPath.empty()) {
                g_browserPath = dir;
            } else {
                g_browserPath = g_browserPath + "/" + dir;
            }
            UpdateBrowserDirs();
        }
    }
    EndScissorMode();
    
    // Scrollbar
    if ((int)g_browserDirs.size() > visibleItems) {
        int sbX = bx + bw - 35;
        int maxScroll = (int)g_browserDirs.size() - visibleItems;
        
        // Draw track
        DrawRectangle(sbX, listY, 12, listH, COL_HOVER);
        
        // Draw thumb
        float ratio = (float)visibleItems / (float)g_browserDirs.size();
        int thumbH = (int)(listH * ratio); if (thumbH < 30) thumbH = 30;
        float scrollRatio = maxScroll > 0 ? (float)g_browserScroll / maxScroll : 0;
        int thumbY = listY + (int)((listH - thumbH) * scrollRatio);
        DrawRectangle(sbX, thumbY, 12, thumbH, COL_ACCENT);
        
        // Handle scrollbar click/drag
        Rectangle sbRect = {(float)sbX, (float)listY, 12, (float)listH};
        if (CheckCollisionPointRec(GetMousePosition(), sbRect) && IsMouseButtonDown(0)) {
            float clickY = GetMousePosition().y - listY - thumbH / 2;
            float clickRatio = clickY / (listH - thumbH);
            if (clickRatio < 0) clickRatio = 0;
            if (clickRatio > 1) clickRatio = 1;
            g_browserScroll = (int)(clickRatio * maxScroll);
        }
    }
    
    bool hasPak = !g_browserPath.empty() && FileExists((g_browserPath + "/pak001.dat").c_str());
    if (hasPak) DrawText("pak001.dat found!", bx + 20, by + bh - 50, 15, COL_DDS);
    
    Rectangle btnSelect = {(float)bx + bw - 220, (float)by + bh - 45, 100, 32};
    Rectangle btnCancel = {(float)bx + bw - 110, (float)by + bh - 45, 90, 32};
    
    DrawRectangleRec(btnSelect, hasPak ? COL_ACCENT : COL_HOVER);
    DrawText("Select", (int)btnSelect.x + 25, (int)btnSelect.y + 8, 15, COL_TEXT);
    DrawRectangleRec(btnCancel, COL_HOVER);
    DrawText("Cancel", (int)btnCancel.x + 20, (int)btnCancel.y + 8, 15, COL_TEXT);
    
    if (hasPak && CheckCollisionPointRec(GetMousePosition(), btnSelect) && IsMouseButtonPressed(0)) {
        g_gameDir = g_browserPath;
        SaveConfig();
        g_pak.OpenGameDir(g_gameDir);
        g_showBrowser = false;
        UpdateFilter();
    }
    if (CheckCollisionPointRec(GetMousePosition(), btnCancel) && IsMouseButtonPressed(0)) g_showBrowser = false;
    if (IsKeyPressed(KEY_ESCAPE)) g_showBrowser = false;
}

//=============================================================================
// DRAW - Main UI
//=============================================================================
void DrawUI() {
    int W = GetScreenWidth(), H = GetScreenHeight();
    int listW = (int)(W * 0.45f);
    int listH = H - 100;
    
    // === LEFT PANEL ===
    DrawRectangle(10, 60, listW, listH, COL_PANEL);
    DrawRectangleLines(10, 60, listW, listH, COL_ACCENT);
    
    // View mode
    Rectangle btnList = {12, 65, 45, 20};
    Rectangle btnTree = {59, 65, 45, 20};
    DrawRectangleRec(btnList, g_viewModeTree ? COL_HOVER : COL_ACCENT);
    DrawRectangleRec(btnTree, g_viewModeTree ? COL_ACCENT : COL_HOVER);
    DrawText("List", 20, 68, 12, COL_TEXT);
    DrawText("Tree", 67, 68, 12, COL_TEXT);
    if (CheckCollisionPointRec(GetMousePosition(), btnList) && IsMouseButtonPressed(0)) { g_viewModeTree = false; }
    if (CheckCollisionPointRec(GetMousePosition(), btnTree) && IsMouseButtonPressed(0)) { g_viewModeTree = true; }
    
    // Filters
    const char* filters[] = {"All", "IMG", "NIF", "SND"};
    for (int i = 0; i < 4; i++) {
        Rectangle btn = {(float)(115 + i * 45), 65, 42, 20};
        DrawRectangleRec(btn, (g_filterType == i) ? COL_ACCENT : COL_HOVER);
        DrawText(filters[i], (int)btn.x + 8, (int)btn.y + 4, 12, COL_TEXT);
        if (CheckCollisionPointRec(GetMousePosition(), btn) && IsMouseButtonPressed(0)) {
            g_filterType = i;
            UpdateFilter();
        }
    }
    
    // Search
    Rectangle searchBox = {310, 65, (float)listW - 310, 20};
    DrawRectangleRec(searchBox, COL_HOVER);
    DrawText(g_searchBuffer[0] ? g_searchBuffer : "Search...", 315, 68, 12, g_searchBuffer[0] ? COL_TEXT : COL_TEXT_DIM);
    
    int key = GetCharPressed();
    while (key > 0) {
        int len = (int)strlen(g_searchBuffer);
        if (key >= 32 && key < 127 && len < 255) {
            g_searchBuffer[len] = (char)key;
            g_searchBuffer[len + 1] = 0;
            g_searchQuery = g_searchBuffer;
            for (char& c : g_searchQuery) c = tolower(c);
            UpdateFilter();
        }
        key = GetCharPressed();
    }
    if (IsKeyPressed(KEY_BACKSPACE) && strlen(g_searchBuffer) > 0) {
        g_searchBuffer[strlen(g_searchBuffer) - 1] = 0;
        g_searchQuery = g_searchBuffer;
        for (char& c : g_searchQuery) c = tolower(c);
        UpdateFilter();
    }
    
    // File list
    int itemH = 20;
    int startY = 92;
    int visibleItems = (listH - 40) / itemH;
    
    int& scrollRef = g_viewModeTree ? g_treeScroll : g_scrollOffset;
    int itemCount = g_viewModeTree ? (int)g_treeItems.size() : (int)g_filteredIndices.size();
    int maxScroll = itemCount - visibleItems;
    if (maxScroll < 0) maxScroll = 0;
    
    float wheel = GetMouseWheelMove();
    Rectangle listRect = {10, (float)startY, (float)listW, (float)(listH - 40)};
    if (wheel != 0 && CheckCollisionPointRec(GetMousePosition(), listRect)) {
        scrollRef -= (int)(wheel * 3);
        if (scrollRef < 0) scrollRef = 0;
        if (scrollRef > maxScroll) scrollRef = maxScroll;
    }
    
    const auto& entries = g_pak.GetEntries();
    
    BeginScissorMode(10, startY, listW, listH - 40);
    
    if (g_viewModeTree) {
        // Tree view
        for (int i = 0; i < visibleItems && (g_treeScroll + i) < (int)g_treeItems.size(); i++) {
            TreeItem& item = g_treeItems[g_treeScroll + i];
            
            Rectangle row = {10, (float)(startY + i * itemH), (float)listW - 15, (float)itemH};
            bool hover = CheckCollisionPointRec(GetMousePosition(), row);
            bool selected = (!item.isFolder && item.entryIndex == g_selectedEntry);
            
            if (selected) DrawRectangleRec(row, COL_SELECTED);
            else if (hover) DrawRectangleRec(row, COL_HOVER);
            
            int indent = 18 + item.depth * 16;
            
            if (item.isFolder) {
                // Folder with expand icon
                const char* icon = item.expanded ? "v" : ">";
                DrawText(icon, indent, (int)row.y + 3, 14, COL_FOLDER);
                DrawText(item.name.c_str(), indent + 14, (int)row.y + 3, 13, COL_FOLDER);
                
                if (hover && IsMouseButtonPressed(0) && item.folderPtr) {
                    item.folderPtr->expanded = !item.folderPtr->expanded;
                    item.expanded = item.folderPtr->expanded;
                    RebuildTreeItems();
                }
            } else {
                // File
                FileType ft = GetFileType(item.name);
                DrawRectangle(indent, (int)row.y + 4, 8, 11, GetFileColor(ft));
                DrawText(item.name.c_str(), indent + 12, (int)row.y + 3, 13, COL_TEXT);
                
                if (hover && IsMouseButtonPressed(0)) {
                    g_selectedEntry = item.entryIndex;
                    LoadPreview(entries[item.entryIndex]);
                }
            }
        }
    } else {
        // List view
        for (int i = 0; i < visibleItems && (g_scrollOffset + i) < (int)g_filteredIndices.size(); i++) {
            int idx = (int)g_filteredIndices[g_scrollOffset + i];
            const auto& e = entries[idx];
            
            Rectangle row = {10, (float)(startY + i * itemH), (float)listW - 15, (float)itemH};
            bool hover = CheckCollisionPointRec(GetMousePosition(), row);
            bool selected = (g_selectedEntry == idx);
            
            if (selected) DrawRectangleRec(row, COL_SELECTED);
            else if (hover) DrawRectangleRec(row, COL_HOVER);
            
            FileType ft = GetFileType(e.filename);
            DrawRectangle(18, (int)row.y + 4, 8, 11, GetFileColor(ft));
            DrawText(e.filename.c_str(), 30, (int)row.y + 3, 13, COL_TEXT);
            DrawText(TextFormat("%dKB", e.size / 1024), listW - 50, (int)row.y + 3, 11, COL_TEXT_DIM);
            
            if (hover && IsMouseButtonPressed(0)) {
                g_selectedEntry = idx;
                LoadPreview(e);
            }
        }
    }
    EndScissorMode();
    
    // Scrollbar with click/drag support
    if (itemCount > visibleItems) {
        float sbH = (float)(listH - 40);
        float ratio = (float)visibleItems / (float)itemCount;
        int thumbH = (int)(sbH * ratio); if (thumbH < 20) thumbH = 20;
        float scrollRatio = maxScroll > 0 ? (float)scrollRef / maxScroll : 0;
        int thumbY = startY + (int)((sbH - thumbH) * scrollRatio);
        
        Rectangle sbRect = {(float)listW, (float)startY, 8, sbH};
        Rectangle thumbRect = {(float)listW, (float)thumbY, 8, (float)thumbH};
        Vector2 mouse = GetMousePosition();
        
        // Start drag
        if (IsMouseButtonPressed(0) && CheckCollisionPointRec(mouse, sbRect)) {
            g_draggingScrollbar = true;
            g_scrollDragStartY = mouse.y;
            g_scrollDragStartVal = scrollRef;
        }
        
        // Continue drag
        if (g_draggingScrollbar && IsMouseButtonDown(0)) {
            float dy = mouse.y - g_scrollDragStartY;
            float scrollPerPixel = (float)maxScroll / (sbH - thumbH);
            scrollRef = g_scrollDragStartVal + (int)(dy * scrollPerPixel);
            if (scrollRef < 0) scrollRef = 0;
            if (scrollRef > maxScroll) scrollRef = maxScroll;
        }
        
        // End drag
        if (IsMouseButtonReleased(0)) {
            g_draggingScrollbar = false;
        }
        
        DrawRectangle(listW, startY, 8, (int)sbH, COL_HOVER);
        DrawRectangle(listW, thumbY, 8, thumbH, g_draggingScrollbar ? WHITE : COL_ACCENT);
    } else {
        g_draggingScrollbar = false;
    }
    
    // File count
    DrawText(TextFormat("%d items", itemCount), 12, H - 35, 12, COL_TEXT_DIM);
    
    // === RIGHT PANEL - Preview ===
    int previewX = listW + 25;
    int previewW = W - previewX - 10;
    
    DrawRectangle(previewX, 60, previewW, listH, COL_PANEL);
    DrawRectangleLines(previewX, 60, previewW, listH, COL_ACCENT);
    DrawText("Preview", previewX + 10, 65, 16, COL_TEXT);
    
    int previewAreaY = 90;
    int previewAreaH = listH - 200;
    
    // Texture preview
    if (g_hasTexturePreview && g_previewTexture.id > 0) {
        int texX = previewX + (previewW - g_previewTexture.width) / 2;
        int texY = previewAreaY + (previewAreaH - g_previewTexture.height) / 2;
        DrawTexture(g_previewTexture, texX, texY, WHITE);
        DrawRectangleLines(texX - 1, texY - 1, g_previewTexture.width + 2, g_previewTexture.height + 2, COL_ACCENT);
    }
    
    // 3D Model preview
    if (g_hasModelPreview && !g_previewModels.empty()) {
        Rectangle previewRect = {(float)previewX + 10, (float)previewAreaY, (float)previewW - 20, (float)previewAreaH};
        
        // Mouse interaction
        if (CheckCollisionPointRec(GetMousePosition(), previewRect)) {
            if (IsMouseButtonDown(0)) {
                g_modelRotY += GetMouseDelta().x * 0.5f;
            }
            g_modelZoom -= GetMouseWheelMove() * g_modelZoom * 0.1f;
            if (g_modelZoom < 1) g_modelZoom = 1;
            if (g_modelZoom > 500) g_modelZoom = 500;
        }
        
        // Resize render texture if needed
        int rtW = previewW - 20;
        int rtH = previewAreaH;
        if (g_modelRenderTex.id == 0 || g_modelRenderTex.texture.width != rtW || g_modelRenderTex.texture.height != rtH) {
            if (g_modelRenderTex.id != 0) UnloadRenderTexture(g_modelRenderTex);
            g_modelRenderTex = LoadRenderTexture(rtW, rtH);
        }
        
        Vector3 center = {
            (g_modelBounds.min.x + g_modelBounds.max.x) / 2,
            (g_modelBounds.min.y + g_modelBounds.max.y) / 2,
            (g_modelBounds.min.z + g_modelBounds.max.z) / 2
        };
        
        g_previewCam.position = {
            center.x + sinf(g_modelRotY * DEG2RAD) * g_modelZoom,
            center.y + g_modelZoom * 0.4f,
            center.z + cosf(g_modelRotY * DEG2RAD) * g_modelZoom
        };
        g_previewCam.target = center;
        g_previewCam.up = {0, 1, 0};
        g_previewCam.fovy = 45.0f;
        g_previewCam.projection = CAMERA_PERSPECTIVE;
        
        BeginTextureMode(g_modelRenderTex);
        ClearBackground({45, 50, 60, 255});
        BeginMode3D(g_previewCam);
        
        DrawGrid(20, 1.0f);
        
        for (auto& m : g_previewModels) {
            DrawModel(m, {0, 0, 0}, 1.0f, WHITE);
        }
        
        EndMode3D();
        EndTextureMode();
        
        // Draw flipped
        DrawTextureRec(g_modelRenderTex.texture, 
            {0, 0, (float)rtW, -(float)rtH},
            {(float)previewX + 10, (float)previewAreaY}, WHITE);
        
        DrawRectangleLines(previewX + 10, previewAreaY, rtW, rtH, COL_ACCENT);
        DrawText("Drag: Rotate | Scroll: Zoom", previewX + 15, previewAreaY + 5, 11, Fade(WHITE, 0.5f));
    }
    
    // Audio preview
    if (g_hasSound || g_hasMusic) {
        int audioY = previewAreaY + previewAreaH / 2 - 25;
        
        Rectangle btnPlay = {(float)previewX + previewW/2 - 45, (float)audioY, 90, 35};
        DrawRectangleRec(btnPlay, g_audioPlaying ? COL_SELECTED : COL_ACCENT);
        DrawText(g_audioPlaying ? "Stop" : "Play", (int)btnPlay.x + 28, (int)btnPlay.y + 9, 15, COL_TEXT);
        
        if (CheckCollisionPointRec(GetMousePosition(), btnPlay) && IsMouseButtonPressed(0)) {
            if (g_audioPlaying) {
                if (g_hasSound) StopSound(g_previewSound);
                if (g_hasMusic) StopMusicStream(g_previewMusic);
                g_audioPlaying = false;
            } else {
                if (g_hasSound) PlaySound(g_previewSound);
                if (g_hasMusic) PlayMusicStream(g_previewMusic);
                g_audioPlaying = true;
            }
        }
        
        DrawText(TextFormat("Duration: %.2fs", g_audioDuration), previewX + previewW/2 - 55, audioY + 45, 13, COL_TEXT_DIM);
    }
    
    // Info text
    if (!g_previewInfo.empty()) {
        int infoY = H - 160;
        int lineY = infoY;
        std::string line;
        for (char c : g_previewInfo) {
            if (c == '\n') {
                DrawText(line.c_str(), previewX + 12, lineY, 12, COL_TEXT_DIM);
                lineY += 14;
                line.clear();
            } else line += c;
        }
        if (!line.empty()) DrawText(line.c_str(), previewX + 12, lineY, 12, COL_TEXT_DIM);
    }
    
    // Extract button
    if (g_selectedEntry >= 0) {
        Rectangle btnExtract = {(float)previewX + 12, (float)H - 85, 90, 26};
        DrawRectangleRec(btnExtract, COL_ACCENT);
        DrawText("Extract", (int)btnExtract.x + 18, (int)btnExtract.y + 5, 14, COL_TEXT);
        
        if (CheckCollisionPointRec(GetMousePosition(), btnExtract) && IsMouseButtonPressed(0)) {
            ExtractFile(entries[g_selectedEntry]);
        }
    }
    
    // === HEADER ===
    DrawRectangle(0, 0, W, 50, COL_PANEL);
    DrawText("KnC DAT Manager", 15, 12, 22, COL_ACCENT);
    
    if (g_pak.GetEntryCount() > 0) {
        DrawText(TextFormat("%d files", (int)g_pak.GetEntryCount()), 200, 18, 13, COL_TEXT_DIM);
    }
    
    Rectangle btnFolder = {(float)W - 130, 10, 120, 30};
    DrawRectangleRec(btnFolder, COL_HOVER);
    DrawText("Change Folder", (int)btnFolder.x + 10, (int)btnFolder.y + 8, 13, COL_TEXT);
    
    if (CheckCollisionPointRec(GetMousePosition(), btnFolder) && IsMouseButtonPressed(0)) {
        g_browserPath = g_gameDir;
        UpdateBrowserDirs();
        g_showBrowser = true;
    }
}

//=============================================================================
// MAIN
//=============================================================================
int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT);
    InitWindow(1500, 950, "KnC DAT Manager");
    InitAudioDevice();
    SetTargetFPS(60);
    SetExitKey(0);
    
    LoadConfig();
    
    if (g_gameDir.empty()) {
        g_showBrowser = true;
        UpdateBrowserDirs();
    } else {
        if (g_pak.OpenGameDir(g_gameDir)) UpdateFilter();
        else { g_showBrowser = true; UpdateBrowserDirs(); }
    }
    
    while (!WindowShouldClose()) {
        if (g_hasMusic && g_audioPlaying) {
            UpdateMusicStream(g_previewMusic);
            if (!IsMusicStreamPlaying(g_previewMusic)) g_audioPlaying = false;
        }
        if (g_hasSound && g_audioPlaying && !IsSoundPlaying(g_previewSound)) g_audioPlaying = false;
        
        BeginDrawing();
        ClearBackground(COL_BG);
        
        if (g_pak.GetEntryCount() == 0 && !g_showBrowser) {
            DrawText("No PAK files loaded", GetScreenWidth()/2 - 90, GetScreenHeight()/2 - 15, 22, COL_TEXT);
            
            DrawRectangle(0, 0, GetScreenWidth(), 50, COL_PANEL);
            DrawText("KnC DAT Manager", 15, 12, 22, COL_ACCENT);
            
            Rectangle btnFolder = {(float)GetScreenWidth() - 130, 10, 120, 30};
            DrawRectangleRec(btnFolder, COL_HOVER);
            DrawText("Change Folder", (int)btnFolder.x + 10, (int)btnFolder.y + 8, 13, COL_TEXT);
            if (CheckCollisionPointRec(GetMousePosition(), btnFolder) && IsMouseButtonPressed(0)) {
                UpdateBrowserDirs();
                g_showBrowser = true;
            }
        } else {
            DrawUI();
        }
        
        if (g_showBrowser) DrawFolderBrowser();
        
        EndDrawing();
    }
    
    ClearPreview();
    if (g_modelRenderTex.id != 0) UnloadRenderTexture(g_modelRenderTex);
    
    for (auto& p : g_texCache) {
        if (p.second.id > 0) UnloadTexture(p.second);
    }
    
    g_pak.Close();
    CloseAudioDevice();
    CloseWindow();
    
    remove("temp_audio.ogg");
    remove("temp_audio.wav");
    
    return 0;
}
