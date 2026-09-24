#include "RoomCraftScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "assets/NifEmbeddedTextures.h"
#include "engine/formats/nif_reader.h"
#include "engine/formats/nif_scene_graph.h"
#include "engine/render/map_scene.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "race/TrackData.h"
#include "tools/track_scene/ghost_car.h"
#include "ui/MenuFrame.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace KnC::Client {

namespace {

// sub 434C00 the top at 25 50 the back at 37 88
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;
// the 3D field of the stage set rect 56 175 of 903 by 530
constexpr Rect kField = {56.f, 175.f, 903.f, 530.f};
// the camera of the stage eye minus 105 0 15 target 120 0 0 field 1 0 radians
constexpr float kEye[3] = {-105.f, 0.f, 15.f};
constexpr float kLook[3] = {120.f, 0.f, 0.f};
constexpr float kFieldRadians = 1.f;
// FUN 00433CB0 the pan step is 0 5 a frame and the eye stays inside this box
constexpr float kPanStep = 0.5f;
constexpr float kPanMaxX = 2.f;
constexpr float kPanMinX = -108.5f;
constexpr float kPanMaxY = 62.f;
constexpr float kPanMinY = -61.f;
// the three helper nifs of the init the two reds ride the moved object the screen rides a back object
constexpr float kMarkerScale = 3.f;
// the marker shapes carry no texture and no material read so the ring takes a flat red
constexpr uint32_t kMarkerTint = 0xff2020ffu;
constexpr float kScreenScale = 4.f;
// FUN 004348F0 a back object puts the lobby screen at x 50 the row y and z 10
constexpr float kScreenX = 50.f;
constexpr float kScreenZ = 10.f;
// the owned strip eleven tiles at 214 plus 54 each the icon on 120 the hit box 48 by 47
constexpr float kStripX = 214.f;
constexpr float kStripStep = 54.f;
constexpr float kStripY = 120.f;
constexpr float kStripW = 48.f;
constexpr float kStripH = 47.f;
constexpr int kStripTiles = 11;
// the six kind tabs on 93 at 266 plus 81 each
constexpr float kKindX = 266.f;
constexpr float kKindStep = 81.f;
constexpr float kKindY = 93.f;
// the info panel at 56 175 the icon at 64 184 the name at 192 186
constexpr float kInfoX = 56.f;
constexpr float kInfoY = 175.f;
// the placed counter of the stage placed on 58 144 the cap 50 on 116 144
constexpr float kCountX = 58.f;
constexpr float kCountY = 144.f;
constexpr int kCategoryCap = 50;
// the tip sheet of button five at 472 333
constexpr float kTipX = 472.f;
constexpr float kTipY = 333.f;
// FUN 004346a0 code 4 and 5 turn by two times the step the keys hold the step at one
constexpr float kYawStep = 2.f;
// FUN 00436050 message 0x200 with MK RBUTTON the step is the pixel run times 0 1
constexpr float kYawPerPixel = 0.1f;
constexpr float kDegToRad = 3.14159265f / 180.f;

struct CraftButton {
    const char* id;
    const char* action;
    const char* art;
    float x;
    float y;
};

const CraftButton kButtons[] = {
    {"btn_save", "save", "Buttons/Common_Room_Save_", 850.f, 140.f},
    {"btn_thumb_left", "thumb_left", "RoomEditer/Factory_Room_Thumbnail_Left_", 177.f, 117.f},
    {"btn_thumb_right", "thumb_right", "RoomEditer/Factory_Room_Thumbnail_Right_", 813.f, 117.f},
    {"btn_tip", "tip", "RoomEditer/Factory_Room_Tip_", 933.f, 97.f},
    {"btn_del", "del", "RoomEditer/Factory_Room_ItemInfo_Del_", 241.f, 211.f},
    {"btn_info", "info", "RoomEditer/Factory_Car_List_Info_", 214.f, 211.f},
};

const char* const kKindArt[6] = {
    "RoomEditer/Factory_Room_Thumbnail_All_", "RoomEditer/Factory_Room_Thumbnail_Sky_",
    "RoomEditer/Factory_Room_Thumbnail_Terrain_", "RoomEditer/Factory_Room_Thumbnail_BackGround_",
    "RoomEditer/Factory_Room_Thumbnail_Object_", "RoomEditer/Factory_Room_Thumbnail_Effect_",
};

// the first nif of a folder the object folders name their nif after themselves with a few odd ones
std::string firstNif(const std::string& dir) {
    std::error_code ignored;
    if (!std::filesystem::is_directory(dir, ignored)) return std::string();
    for (const auto& entry : std::filesystem::directory_iterator(dir, ignored)) {
        std::string ext = entry.path().extension().string();
        for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (ext == ".nif") return entry.path().string();
    }
    return std::string();
}

// the dds copies of the old room nifs land in a cache under the temp folder
std::string textureCache(const std::string& nifPath) {
    std::error_code ignored;
    const std::filesystem::path nif(nifPath);
    return (std::filesystem::temp_directory_path(ignored) / "knc_client" / "nif_textures" /
            nif.parent_path().filename() / nif.stem())
        .string();
}

// the two marker nifs carry no texture and no uv so the prop builder drops them this walk keeps them
bool loadUntexturedModel(const std::string& path, uint32_t tint, KnC::Render::PropModel& out) {
    KnC::NifScene scene;
    std::string error;
    if (!KnC::read_nif_scene(path, scene, error)) {
        std::printf("[roomcraft] %s failed %s\n", path.c_str(), error.c_str());
        return false;
    }
    KnC::Render::PropPart part;
    part.has_vertex_colours = true;
    // the stock marker is an emissive material so the ring saturates with no light of the room
    part.surface.has_material = true;
    part.surface.has_vertex_colour = true;
    for (int axis = 0; axis < 3; ++axis) {
        part.surface.material.ambient[axis] = 0.f;
        part.surface.material.diffuse[axis] = 0.f;
        part.surface.material.emissive[axis] = 1.f;
    }
    part.surface.vertex_colour.lighting = KnC::NifLightingMode::EmissiveOnly;
    part.surface.vertex_colour.vertex = KnC::NifVertexMode::Emissive;
    float low[3] = {1e30f, 1e30f, 1e30f};
    float high[3] = {-1e30f, -1e30f, -1e30f};
    struct Step {
        uint32_t block;
        KnC::NifPlacement placement;
    };
    std::vector<Step> stack;
    for (uint32_t root : KnC::find_root_block_indices(scene)) stack.push_back({root, KnC::NifPlacement{}});
    while (!stack.empty()) {
        const Step step = stack.back();
        stack.pop_back();
        if (step.block >= scene.blocks.size()) continue;
        const KnC::NifBlock& block = scene.blocks[step.block];
        KnC::NifPlacement here = step.placement;
        if (block.has_transform) here = KnC::compose_placement(step.placement, block.transform);
        for (uint32_t child : block.children) stack.push_back({child, here});
        if (block.data_link >= scene.blocks.size()) continue;
        const KnC::NifBlock& mesh = scene.blocks[block.data_link];
        if (mesh.vertices.empty() || mesh.triangles.empty()) continue;
        const uint32_t base = static_cast<uint32_t>(part.vertices.size());
        const size_t count = mesh.vertices.size() / 3;
        for (size_t i = 0; i < count; ++i) {
            const float local[3] = {mesh.vertices[i * 3], mesh.vertices[i * 3 + 1], mesh.vertices[i * 3 + 2]};
            float world[3];
            KnC::place_point(here, local, world);
            KnC::Render::SceneVertex v;
            v.abgr = tint;
            v.x = world[0];
            v.y = world[1];
            v.z = world[2];
            // a shape with no authored normal faces up so the sun term never normalises a zero vector
            v.normal_z = 1.f;
            if (mesh.normals.size() >= (i + 1) * 3) {
                const float n[3] = {mesh.normals[i * 3], mesh.normals[i * 3 + 1], mesh.normals[i * 3 + 2]};
                float dir[3];
                KnC::place_direction(here, n, dir);
                v.normal_x = dir[0];
                v.normal_y = dir[1];
                v.normal_z = dir[2];
            }
            // the shapes are authored black the reader drops their red material so the caller tint stands in
            for (int axis = 0; axis < 3; ++axis) {
                low[axis] = std::min(low[axis], world[axis]);
                high[axis] = std::max(high[axis], world[axis]);
            }
            part.vertices.push_back(v);
        }
        for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3) {
            part.indices.push_back(base + mesh.triangles[i]);
            part.indices.push_back(base + mesh.triangles[i + 1]);
            part.indices.push_back(base + mesh.triangles[i + 2]);
        }
    }
    if (part.vertices.empty()) return false;
    float radius = 0.f;
    for (int axis = 0; axis < 3; ++axis) {
        part.bound.center[axis] = (low[axis] + high[axis]) * 0.5f;
        radius = std::max(radius, (high[axis] - low[axis]) * 0.5f);
    }
    part.bound.radius = radius;
    out.bound = part.bound;
    out.lit_by_map_ambient = false;
    out.parts.push_back(std::move(part));
    return true;
}

// the nif of one owned row by its category empty when the folder ships none
std::string objectNif(const std::string& root, uint32_t category, const std::string& folder, std::string& dir) {
    switch (category) {
    case 0: dir = root + "Sky/" + folder; return findEntryCi(dir, "sky.nif");
    case 2: dir = root + "BgObj/" + folder; return findEntryCi(dir, "backGround.nif");
    case 3: {
        dir = root + "Object/" + folder;
        const std::string named = findEntryCi(dir, folder + ".nif");
        return named.empty() ? firstNif(dir) : named;
    }
    case 4: dir = root + "Effect/" + folder; return findEntryCi(dir, "effect.nif");
    default: break;
    }
    return std::string();
}

}

void RoomCraftScreen::enter() {
    AssetStore& assets = m_app.assets();
    addMenuFrame(*this, assets, FrameMode::Full, "roomcraft");
    auto button = [&](const std::string& id, const std::string& action, const std::string& art, float x, float y) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = id;
        b->action = action;
        b->normal = assets.texture(art + "00.png");
        b->hover = assets.texture(art + "01.png");
        b->pressed = assets.texture(art + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {x, y, size ? static_cast<float>(size->width) : 60.f, size ? static_cast<float>(size->height) : 24.f};
        b->zIndex = 30;
        add(std::move(b));
    };
    for (const CraftButton& c : kButtons) button(c.id, c.action, c.art, c.x, c.y);
    for (int i = 0; i < 6; ++i)
        button("btn_kind" + std::to_string(i), "kind" + std::to_string(i), kKindArt[i],
               kKindX + kKindStep * static_cast<float>(i), kKindY);
    m_kind = 0;
    m_scroll = 0;
    m_tile = -1;
    m_selected = -1;
    m_dragging = false;
    m_tip = false;
    m_time = 0.f;
    m_captured = false;
    m_step = 0;
    m_status.clear();
    // the stock stage opens on the 0x010E request the catalogue and the owned rows come before the ack
    if (!m_opened) {
        m_opened = true;
        m_app.session().openRoomCraft();
    }
    takeRows();
    for (int i = 0; i < 3; ++i) { m_eye[i] = kEye[i]; m_look[i] = kLook[i]; }
    m_panning = false;
    m_turning = false;
    m_worldReady = loadField();
    refreshFieldProps();
}

void RoomCraftScreen::leave() {
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

// the working copy the stage edits the catalogue keeps the master the save diffs the two
void RoomCraftScreen::takeRows() {
    m_rows = m_app.session().catalog().roomCraftInstances();
    refreshTiles();
}

int RoomCraftScreen::rowOfInstance(uint32_t instance) const {
    for (size_t i = 0; i < m_rows.size(); ++i) if (m_rows[i].instance == instance) return static_cast<int>(i);
    return -1;
}

int RoomCraftScreen::placedOfKey(uint32_t objectKey) const {
    int count = 0;
    for (const RoomCraftInstance& r : m_rows) if (r.objectKey == objectKey && r.placed != 0) ++count;
    return count;
}

int RoomCraftScreen::placedOfCategory(uint32_t category) const {
    int count = 0;
    for (const RoomCraftInstance& r : m_rows) if (r.category == category && r.placed != 0) ++count;
    return count;
}

// one tile per owned object key of the picked kind the count is owned minus dead minus placed
void RoomCraftScreen::refreshTiles() {
    m_tiles.clear();
    const Catalog& cat = m_app.session().catalog();
    for (const RoomCraftInstance& r : m_rows) {
        if (r.active == 0) continue;
        if (m_kind != 0 && r.category != static_cast<uint32_t>(m_kind - 1)) continue;
        StripTile* tile = nullptr;
        for (StripTile& t : m_tiles) if (t.objectKey == r.objectKey) tile = &t;
        if (!tile) {
            StripTile fresh;
            fresh.objectKey = r.objectKey;
            fresh.category = r.category;
            if (const RoomObjectRow* def = cat.roomObject(r.objectKey)) {
                fresh.folder = def->folder;
                fresh.nameKey = def->nameKey;
            }
            m_tiles.push_back(fresh);
            tile = &m_tiles.back();
        }
        if (r.placed != 0) ++tile->placed;
        else ++tile->available;
    }
    const int pages = std::max(0, static_cast<int>(m_tiles.size()) - kStripTiles);
    if (m_scroll > pages) m_scroll = pages;
    if (m_scroll < 0) m_scroll = 0;
    if (m_tile >= static_cast<int>(m_tiles.size())) m_tile = -1;
}

std::string RoomCraftScreen::worldToken() const {
    std::string token;
    for (const RoomCraftInstance& r : m_rows) {
        if (r.placed == 0 || r.category == 3) continue;
        token += std::to_string(r.category) + ":" + std::to_string(r.objectKey) + " ";
    }
    return token;
}

// sub 488D60 the placed floor brings the track the sky the back object and the effect ride on it
bool RoomCraftScreen::loadField() {
    const Catalog& cat = m_app.session().catalog();
    const std::string root = m_app.options().gameDir + "/Data/Public/World/Room/";
    m_worldToken = worldToken();
    std::string floor = "Floor01";
    for (const RoomCraftInstance& r : m_rows) {
        if (r.category != 1 || r.placed == 0) continue;
        if (const RoomObjectRow* def = cat.roomObject(r.objectKey)) if (!def->folder.empty()) floor = def->folder;
    }
    TrackFiles files;
    files.trackDir = root + "Floor/" + floor;
    files.mapDir = files.trackDir;
    files.themeFolder = "Room";
    files.trackFolder = floor;
    m_world = RaceWorld();
    std::string error;
    if (!loadRaceWorld(files, m_world, error)) {
        std::printf("[roomcraft] floor %s failed %s\n", floor.c_str(), error.c_str());
        return false;
    }
    KnC::Render::MapScene& scene = m_world.scene.scene;
    const std::string floorNif = findEntryCi(files.trackDir, "track.nif");
    if (!floorNif.empty())
        applyEmbeddedTextures(extractEmbeddedTextures(floorNif, textureCache(floorNif)), scene);
    // the sky the back object and the effect stand still so they ride in the scene itself
    for (const RoomCraftInstance& r : m_rows) {
        if (r.placed == 0 || r.category == 1 || r.category == 3) continue;
        const RoomObjectRow* def = cat.roomObject(r.objectKey);
        if (!def) continue;
        std::string dir;
        const std::string nif = objectNif(root, r.category, def->folder, dir);
        if (nif.empty()) continue;
        KnC::Render::NifModelRequest request;
        request.nif_path = nif;
        request.texture_dir = dir;
        KnC::Render::PropModel model;
        std::string modelError;
        if (!KnC::Render::load_prop_model(request, model, modelError)) continue;
        KnC::Tools::resolve_textures(dir, model);
        applyEmbeddedTextures(extractEmbeddedTextures(nif, textureCache(nif)), model);
        KnC::Render::PropInstance instance;
        instance.model_index = scene.prop_models.size();
        scene.prop_models.push_back(std::move(model));
        scene.prop_instances.push_back(instance);
    }
    for (KnC::Render::PropModel& model : scene.prop_models) KnC::Tools::resolve_textures(root, model);
    m_propToken.clear();
    m_fieldProps.clear();
    m_markersLoaded = false;
    std::printf("[roomcraft] field floor %s with %zu scene props\n", floor.c_str(), scene.prop_models.size());
    return m_view.load(m_app.renderer(), m_world);
}

// one appended nif per placed object row the move and the turn only change its matrix
void RoomCraftScreen::refreshFieldProps() {
    if (!m_worldReady) return;
    std::string token;
    for (const RoomCraftInstance& r : m_rows)
        if (r.category == 3 && r.placed != 0) token += std::to_string(r.instance) + " ";
    if (token == m_propToken) return;
    m_propToken = token;
    const Catalog& cat = m_app.session().catalog();
    const std::string root = m_app.options().gameDir + "/Data/Public/World/Room/";
    for (const RoomCraftInstance& r : m_rows) {
        if (r.category != 3 || r.placed == 0) continue;
        bool known = false;
        for (const FieldProp& p : m_fieldProps) if (p.instance == r.instance) known = true;
        if (known) continue;
        const RoomObjectRow* def = cat.roomObject(r.objectKey);
        if (!def) continue;
        std::string dir;
        const std::string nif = objectNif(root, 3, def->folder, dir);
        if (nif.empty()) { std::printf("[roomcraft] object %s has no nif\n", def->folder.c_str()); continue; }
        KnC::Render::NifModelRequest request;
        request.nif_path = nif;
        request.texture_dir = dir;
        KnC::Render::PropModel model;
        std::string modelError;
        if (!KnC::Render::load_prop_model(request, model, modelError)) continue;
        KnC::Tools::resolve_textures(dir, model);
        KnC::Tools::resolve_textures(root, model);
        applyEmbeddedTextures(extractEmbeddedTextures(nif, textureCache(nif)), model);
        FieldProp prop;
        prop.instance = r.instance;
        prop.objectKey = r.objectKey;
        prop.radius = model.bound.radius;
        prop.handle = m_view.addProp(m_app.renderer(), model);
        m_fieldProps.push_back(prop);
        std::printf("[roomcraft] object %s appended as prop %d\n", def->folder.c_str(), prop.handle);
    }
}

// FUN 004E3C40 the corner test ours has no COL nodes so the loaded prop bound stands in for them
bool RoomCraftScreen::blockedPlacement(const RoomCraftInstance& row) const {
    const KnC::Render::MapScene& scene = m_world.scene.scene;
    if (scene.bounds_max[0] <= scene.bounds_min[0]) return false;
    float radius = 0.f;
    for (const FieldProp& p : m_fieldProps) if (p.instance == row.instance) radius = p.radius;
    return row.x - radius < scene.bounds_min[0] || row.x + radius > scene.bounds_max[0] ||
           row.y - radius < scene.bounds_min[1] || row.y + radius > scene.bounds_max[1];
}

// sub 434C00 loads lobby image red and red2 at scale three lobby screen at scale four and PICK
void RoomCraftScreen::loadMarkers() {
    m_markerOk = -1;
    m_markerBad = -1;
    m_markerScreen = -1;
    const std::string root = m_app.options().gameDir + "/Data/Public/World/Room/";
    auto marker = [&](const char* file) {
        const std::string nif = findEntryCi(root, file);
        if (nif.empty()) { std::printf("[roomcraft] marker %s is not in the room folder\n", file); return -1; }
        KnC::Render::NifModelRequest request;
        request.nif_path = nif;
        request.texture_dir = root;
        KnC::Render::PropModel model;
        std::string error;
        if (!KnC::Render::load_prop_model(request, model, error)) {
            std::printf("[roomcraft] marker %s failed %s\n", file, error.c_str());
            return -1;
        }
        KnC::Tools::resolve_textures(root, model);
        applyEmbeddedTextures(extractEmbeddedTextures(nif, textureCache(nif)), model);
        // the material colour of these shapes is not read so the marker takes the red of its name
        if (model.parts.empty() && !loadUntexturedModel(nif, kMarkerTint, model)) return -1;
        std::printf("[roomcraft] marker %s %zu parts bound %.2f %.2f %.2f r %.2f\n", file, model.parts.size(),
                    model.bound.center[0], model.bound.center[1], model.bound.center[2], model.bound.radius);
        return m_view.addProp(m_app.renderer(), model);
    };
    m_markerOk = marker("lobby_image_red.nif");
    m_markerBad = marker("lobby_image_red2.nif");
    m_markerScreen = marker("lobby_screen.nif");
    // the PICK nif is the ray surface for drags its low face sets the drag plane on the floor
    m_pickZ = 0.f;
    const std::string pick = findEntryCi(root, "PICK.nif");
    if (!pick.empty()) {
        KnC::Render::NifModelRequest request;
        request.nif_path = pick;
        request.texture_dir = root;
        KnC::Render::PropModel model;
        std::string error;
        if (KnC::Render::load_prop_model(request, model, error)) {
            const float low = model.bound.center[2] - model.bound.radius;
            const KnC::Render::MapScene& scene = m_world.scene.scene;
            if (low >= scene.bounds_min[2] && low <= scene.bounds_max[2]) m_pickZ = low;
            std::printf("[roomcraft] PICK bound z %.2f radius %.2f drag plane %.2f\n", model.bound.center[2],
                        model.bound.radius, m_pickZ);
        }
    }
    m_markersLoaded = true;
    std::printf("[roomcraft] markers ok %d bad %d screen %d pick plane z %.2f\n", m_markerOk, m_markerBad,
                m_markerScreen, m_pickZ);
}

// FUN 004348F0 the two reds ride the picked object the screen rides a picked back object
void RoomCraftScreen::placeMarkers() {
    static const float kHidden[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    auto hide = [&](int handle) { if (handle >= 0) m_view.placeProp(handle, kHidden, false); };
    if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) {
        hide(m_markerOk);
        hide(m_markerBad);
        hide(m_markerScreen);
        return;
    }
    const RoomCraftInstance& r = m_rows[static_cast<size_t>(m_selected)];
    if (r.placed == 0) { hide(m_markerOk); hide(m_markerBad); hide(m_markerScreen); return; }
    auto place = [&](int handle, float scale, float x, float y, float z) {
        if (handle < 0) return;
        float world[16];
        bx::mtxScale(world, scale, scale, scale);
        world[12] = x;
        world[13] = y;
        world[14] = z;
        m_view.placeProp(handle, world, true);
    };
    if (r.category == 2) {
        hide(m_markerOk);
        hide(m_markerBad);
        place(m_markerScreen, kScreenScale, kScreenX, r.y, kScreenZ);
        return;
    }
    hide(m_markerScreen);
    if (r.category != 3) { hide(m_markerOk); hide(m_markerBad); return; }
    // FUN 004E3C40 test flag down answers one when a COL corner leaves the field ours takes the bound
    const bool blocked = blockedPlacement(r);
    if (blocked) {
        hide(m_markerOk);
        place(m_markerBad, kMarkerScale, r.x, r.y, r.z);
    } else {
        hide(m_markerBad);
        place(m_markerOk, kMarkerScale, r.x, r.y, r.z);
    }
}

void RoomCraftScreen::panCamera(int code) {
    switch (code) {
    case 1:
        if (m_eye[0] >= kPanMaxX) return;
        m_eye[0] += kPanStep;
        m_look[0] += kPanStep;
        return;
    case 2:
        if (m_eye[0] <= kPanMinX) return;
        m_eye[0] -= kPanStep;
        m_look[0] -= kPanStep;
        return;
    case 3:
        if (m_eye[1] >= kPanMaxY) return;
        m_eye[1] += kPanStep;
        m_look[1] += kPanStep;
        return;
    case 4:
        if (m_eye[1] <= kPanMinY) return;
        m_eye[1] -= kPanStep;
        m_look[1] -= kPanStep;
        return;
    default: return;
    }
}

void RoomCraftScreen::update(float dt) {
    m_time += dt;
    // FUN 00435FE0 runs one pan step a frame while the middle button holds its code
    if (m_panning && m_panCode != 0) {
        panCamera(m_panCode);
        std::printf("[roomcraft] pan code %d eye %.1f %.1f look %.1f %.1f\n", m_panCode, m_eye[0], m_eye[1], m_look[0], m_look[1]);
    }
    if (!m_app.captureMode() || m_app.scripted()) return;
    if (m_time < static_cast<float>(m_step) * 0.8f + 1.2f) return;
    if (m_step == 0) {
        m_captured = true;
        m_app.captureStage("roomcraft");
        if (m_app.options().stopAt == "roomcraft") m_app.finishRun();
    }
    ++m_step;
}

void RoomCraftScreen::sceneLost() {
    m_worldReady = loadField();
    refreshFieldProps();
    if (m_worldReady) loadMarkers();
}

bool RoomCraftScreen::drawScene() {
    if (!m_worldReady) return false;
    // the canvas rect in framebuffer pixels letterboxed or stretched as the frame is
    float px[4];
    m_app.canvasToPixels(kField.x, kField.y, kField.w, kField.h, px);
    KnC::Render::ViewportRect box;
    box.x = static_cast<uint16_t>(px[0]);
    box.y = static_cast<uint16_t>(px[1]);
    box.width = static_cast<uint16_t>(px[2]);
    box.height = static_cast<uint16_t>(px[3]);
    const KnC::Render::ViewportRect& now = m_app.renderer().viewport();
    if (now.x != box.x || now.y != box.y || now.width != box.width || now.height != box.height)
        m_app.renderer().set_viewport(box);
    for (const FieldProp& p : m_fieldProps) {
        const int row = rowOfInstance(p.instance);
        static const float kHidden[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
        if (row < 0) { m_view.placeProp(p.handle, kHidden, false); continue; }
        const RoomCraftInstance& r = m_rows[static_cast<size_t>(row)];
        float world[16];
        bx::mtxRotateZ(world, r.yaw * kDegToRad);
        world[12] = r.x;
        world[13] = r.y;
        world[14] = r.z;
        m_view.placeProp(p.handle, world, r.placed != 0);
    }
    if (!m_markersLoaded) loadMarkers();
    placeMarkers();
    m_view.drawFixed(m_app.renderer(), m_eye, m_look, kFieldRadians, 1.f / 25.f);
    return true;
}

// the view and the lens of the last field draw project a world point onto the canvas
bool RoomCraftScreen::project(const float world[3], float& sx, float& sy) const {
    float eye[3], view[16], proj[16];
    m_view.lastView(view, eye);
    m_app.renderer().projection(proj);
    float v[4] = {world[0], world[1], world[2], 1.f};
    float e[4], c[4];
    bx::vec4MulMtx(e, v, view);
    bx::vec4MulMtx(c, e, proj);
    if (c[3] <= 0.001f) return false;
    sx = kField.x + (c[0] / c[3] * 0.5f + 0.5f) * kField.w;
    sy = kField.y + (0.5f - c[1] / c[3] * 0.5f) * kField.h;
    return true;
}

// the click ray through the field rect cut on the ground plane the stock picks the floor the same way
bool RoomCraftScreen::groundPoint(float x, float y, float out[3]) const {
    if (!kField.contains(x, y)) return false;
    float eye[3], view[16], proj[16];
    m_view.lastView(view, eye);
    m_app.renderer().projection(proj);
    float viewProj[16], inverse[16];
    bx::mtxMul(viewProj, view, proj);
    bx::mtxInverse(inverse, viewProj);
    const float nx = (x - kField.x) / kField.w * 2.f - 1.f;
    const float ny = 1.f - (y - kField.y) / kField.h * 2.f;
    float nearPoint[4] = {nx, ny, 0.f, 1.f};
    float farPoint[4] = {nx, ny, 1.f, 1.f};
    float a[4], b[4];
    bx::vec4MulMtx(a, nearPoint, inverse);
    bx::vec4MulMtx(b, farPoint, inverse);
    if (std::fabs(a[3]) < 1e-6f || std::fabs(b[3]) < 1e-6f) return false;
    for (int i = 0; i < 3; ++i) { a[i] /= a[3]; b[i] /= b[3]; }
    const float dz = b[2] - a[2];
    if (std::fabs(dz) < 1e-6f) return false;
    // the stock ray picks the World Room PICK nif ours cuts the ray on the plane that nif stands at
    const float t = (m_pickZ - a[2]) / dz;
    if (t < 0.f) return false;
    for (int i = 0; i < 3; ++i) out[i] = a[i] + (b[i] - a[i]) * t;
    out[2] = 0.f;
    // FUN 004E3C40 keeps a placement on the floor so a click near the sky line lands on its edge
    const KnC::Render::MapScene& scene = m_world.scene.scene;
    if (scene.bounds_max[0] > scene.bounds_min[0]) {
        out[0] = std::min(std::max(out[0], scene.bounds_min[0]), scene.bounds_max[0]);
        out[1] = std::min(std::max(out[1], scene.bounds_min[1]), scene.bounds_max[1]);
    }
    return true;
}

// the placed row whose screen point sits nearest the click inside twenty four pixels
int RoomCraftScreen::pickPlaced(float x, float y) const {
    int best = -1;
    float bestDistance = 24.f;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const RoomCraftInstance& r = m_rows[i];
        if (r.placed == 0 || r.category != 3) continue;
        const float world[3] = {r.x, r.y, r.z};
        float sx = 0.f, sy = 0.f;
        if (!project(world, sx, sy)) continue;
        const float distance = std::sqrt((sx - x) * (sx - x) + (sy - y) * (sy - y));
        if (distance >= bestDistance) continue;
        bestDistance = distance;
        best = static_cast<int>(i);
    }
    return best;
}

// FUN 00435810 a tile then a click on the field the singleton kinds take the others off first
void RoomCraftScreen::placeTile(int tile, const float world[3]) {
    if (tile < 0 || tile >= static_cast<int>(m_tiles.size())) return;
    const StripTile& picked = m_tiles[static_cast<size_t>(tile)];
    int row = -1;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        const RoomCraftInstance& r = m_rows[i];
        if (r.objectKey == picked.objectKey && r.placed == 0 && r.active == 1) { row = static_cast<int>(i); break; }
    }
    if (row < 0) { m_status = "no free row of that object is owned"; return; }
    RoomCraftInstance& r = m_rows[static_cast<size_t>(row)];
    if (r.category == 3) {
        // the two caps of 0x435E6E fifty placed of the kind and the max placeable of the def
        if (placedOfCategory(3) >= kCategoryCap) { m_status = m_app.tr("MSG_NOT_SET_ROOM_OBJECT"); return; }
        const RoomObjectRow* def = m_app.session().catalog().roomObject(r.objectKey);
        const int cap = def ? static_cast<int>(def->maxPlaceable) : 0;
        if (cap > 0 && placedOfKey(r.objectKey) >= cap) { m_status = m_app.tr("MSG_NOT_SET_EACH_ROOM_OBJECT"); return; }
        r.x = world[0];
        r.y = world[1];
        r.z = 0.f;
        r.yaw = 0.f;
    } else {
        for (RoomCraftInstance& other : m_rows)
            if (other.category == r.category && other.instance != r.instance) other.placed = 0;
    }
    r.placed = 1;
    m_selected = row;
    m_status = "placed " + picked.folder + " instance " + std::to_string(r.instance);
    refreshTiles();
    if (r.category != 3) { m_worldReady = loadField(); }
    refreshFieldProps();
}

void RoomCraftScreen::moveSelected(const float world[3]) {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) return;
    RoomCraftInstance& r = m_rows[static_cast<size_t>(m_selected)];
    if (r.category != 3 || r.placed == 0) return;
    r.x = world[0];
    r.y = world[1];
    r.z = 0.f;
}

// FUN 004346a0 code 4 and 5 the keys hold the step at one so a press turns two degrees
void RoomCraftScreen::rotateSelected(float degrees) {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) return;
    RoomCraftInstance& r = m_rows[static_cast<size_t>(m_selected)];
    if (r.category != 3 || r.placed == 0) return;
    r.yaw += degrees;
    while (r.yaw >= 360.f) r.yaw -= 360.f;
    while (r.yaw < 0.f) r.yaw += 360.f;
    m_status = "yaw " + std::to_string(static_cast<int>(r.yaw));
}

// FUN 004346a0 code 6 the row keeps its place and its yaw only the placed flag drops
void RoomCraftScreen::removeSelected() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) { m_status = "pick a placed object first"; return; }
    RoomCraftInstance& r = m_rows[static_cast<size_t>(m_selected)];
    if (r.placed == 0) return;
    r.placed = 0;
    m_status = "removed instance " + std::to_string(r.instance);
    const uint32_t category = r.category;
    m_selected = -1;
    refreshTiles();
    if (category != 3) m_worldReady = loadField();
    refreshFieldProps();
}

// 0x4358C2 the dirty diff by index on the position the yaw and the placed flag
void RoomCraftScreen::save() {
    const std::vector<RoomCraftInstance>& master = m_app.session().catalog().roomCraftMaster();
    std::vector<RoomCraftInstance> dirty;
    for (size_t i = 0; i < m_rows.size(); ++i) {
        if (i >= master.size()) { dirty.push_back(m_rows[i]); continue; }
        const RoomCraftInstance& a = m_rows[i];
        const RoomCraftInstance& b = master[i];
        if (a.x == b.x && a.y == b.y && a.z == b.z && a.yaw == b.yaw && a.placed == b.placed) continue;
        dirty.push_back(a);
    }
    if (dirty.empty()) { m_status = m_app.tr("MSG_SAVE_DONE"); return; }
    m_app.session().saveRoomCraft(dirty);
    m_status = "0x010F sent with " + std::to_string(dirty.size()) + " changed rows";
}

void RoomCraftScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    // the wallpaper and the back art keep the field open the 3D lands under the sprites
    drawFrameBack(batch, assets, m_worldReady ? kField : Rect{0.f, 0.f, 0.f, 0.f});
    for (Widget* w : m_order) if (w->visible && w->zIndex < 0 && w->zIndex > -19) w->draw(ctx);
    drawTextureWithHole(batch, assets.texture("RoomEditer/Factory_Room_Back.png"), kBackX, kBackY,
                        m_worldReady ? kField : Rect{0.f, 0.f, 0.f, 0.f});
    sprite("RoomEditer/Factory_Room_Top.png", kTopX, kTopY);
    for (int i = 0; i < 6; ++i)
        if (ButtonWidget* b = findAs<ButtonWidget>("btn_kind" + std::to_string(i)))
            b->normal = assets.texture(std::string(kKindArt[i]) + (i == m_kind ? "01.png" : "00.png"));
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

void RoomCraftScreen::drawOverlay(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    // the owned strip eleven tiles the number on each tile is what is left to place
    const Texture* box = assets.texture("RoomEditer/UI_iconbox_unuse.png");
    const Texture* setup = assets.texture("RoomEditer/UI_SetupItem.png");
    for (int i = 0; i < kStripTiles; ++i) {
        const float x = kStripX + kStripStep * static_cast<float>(i);
        if (box && box->valid()) ctx.batch.draw(box->handle, x, kStripY, static_cast<float>(box->width), static_cast<float>(box->height));
        const size_t index = static_cast<size_t>(m_scroll + i);
        if (index >= m_tiles.size()) continue;
        const StripTile& tile = m_tiles[index];
        const Texture* icon = assets.texture("Parts/" + tile.folder + "_01.png");
        if (icon && icon->valid()) ctx.batch.draw(icon->handle, x + 2.f, kStripY + 2.f, 44.f, 43.f);
        else if (setup && setup->valid()) ctx.batch.draw(setup->handle, x + 4.f, kStripY + 4.f, static_cast<float>(setup->width), static_cast<float>(setup->height));
        if (static_cast<int>(index) == m_tile) ctx.batch.fill(x, kStripY, kStripW, kStripH, rgba(255, 220, 90, 70));
        ctx.bold.draw(ctx.batch, std::to_string(tile.available), x + 25.f, kStripY + 1.f, 13.f, kInkWhite);
    }
    // the info panel of the selected row over the top left of the field
    sprite("RoomEditer/Factory_Room_ItemInfo_Back.png", kInfoX, kInfoY);
    if (m_selected >= 0 && m_selected < static_cast<int>(m_rows.size())) {
        const RoomCraftInstance& r = m_rows[static_cast<size_t>(m_selected)];
        if (const RoomObjectRow* def = m_app.session().catalog().roomObject(r.objectKey)) {
            const Texture* icon = assets.texture("Parts/" + def->folder + "_01.png");
            if (icon && icon->valid()) ctx.batch.draw(icon->handle, kInfoX + 8.f, kInfoY + 9.f, 44.f, 43.f);
            ctx.bold.draw(ctx.batch, m_app.tr(def->nameKey), kInfoX + 136.f, kInfoY + 11.f, 14.f, kInkWhite);
        }
    }
    // the placed counter of the object kind over the cap of fifty
    ctx.bold.draw(ctx.batch, std::to_string(placedOfCategory(3)), kCountX, kCountY, 15.f, kInkWhite);
    ctx.bold.draw(ctx.batch, std::to_string(kCategoryCap), kCountX + 58.f, kCountY, 15.f, kInkWhite);
    // the selected object wears a ring so the move and the turn show which row they hit
    if (m_selected >= 0 && m_selected < static_cast<int>(m_rows.size())) {
        const RoomCraftInstance& r = m_rows[static_cast<size_t>(m_selected)];
        const float world[3] = {r.x, r.y, r.z};
        float sx = 0.f, sy = 0.f;
        if (r.placed != 0 && r.category == 3 && project(world, sx, sy))
            ctx.batch.fill(sx - 6.f, sy - 6.f, 12.f, 12.f, rgba(255, 220, 90, 180));
    }
    if (m_tip) sprite("RoomEditer/Factory_Room_Tip.png", kTipX, kTipY);
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 40.f, 706.f, 12.f, kInkGrey);
}

void RoomCraftScreen::onMouseMove(float x, float y) {
    m_cursorX = x;
    m_cursorY = y;
    // FUN 00436050 message 0x200 with MK MBUTTON the longer run picks the pan code the eye walks per frame
    if (m_panning) {
        const float dx = x - m_panX;
        const float dy = y - m_panY;
        m_panCode = std::fabs(dy) < std::fabs(dx) ? (dx > 0.f ? 4 : (dx < 0.f ? 3 : 0))
                                                  : (dy > 0.f ? 2 : (dy < 0.f ? 1 : 0));
        m_panX = x;
        m_panY = y;
        WidgetScreen::onMouseMove(x, y);
        return;
    }
    // FUN 00436050 with MK RBUTTON the horizontal run turns the picked object 0 2 degrees a pixel
    if (m_turning) {
        const float dx = x - m_turnX;
        const float dy = y - m_turnY;
        m_turnX = x;
        m_turnY = y;
        if (std::fabs(dy) < std::fabs(dx) && dx != 0.f) {
            rotateSelected(kYawStep * std::fabs(dx) * kYawPerPixel * (dx > 0.f ? 1.f : -1.f));
            if (m_selected >= 0 && m_selected < static_cast<int>(m_rows.size()))
                std::printf("[roomcraft] right drag %.0f px yaw %.1f\n", dx, m_rows[static_cast<size_t>(m_selected)].yaw);
        }
        WidgetScreen::onMouseMove(x, y);
        return;
    }
    if (!m_dragging) { WidgetScreen::onMouseMove(x, y); return; }
    float world[3];
    if (groundPoint(x, y, world)) moveSelected(world);
    WidgetScreen::onMouseMove(x, y);
}

void RoomCraftScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        for (int i = 0; i < kStripTiles; ++i) {
            const Rect r = {kStripX + kStripStep * static_cast<float>(i), kStripY + 1.f, kStripW, kStripH};
            if (!r.contains(x, y)) continue;
            const size_t index = static_cast<size_t>(m_scroll + i);
            if (index >= m_tiles.size()) break;
            m_tile = static_cast<int>(index);
            m_app.click();
            m_status = "picked " + m_tiles[index].folder + ", click the field to place it";
            return;
        }
        if (kField.contains(x, y)) {
            const int hit = pickPlaced(x, y);
            if (hit >= 0) {
                m_selected = hit;
                m_dragging = true;
                m_status = "instance " + std::to_string(m_rows[static_cast<size_t>(hit)].instance) + " grabbed";
                return;
            }
            float world[3];
            if (m_tile >= 0 && groundPoint(x, y, world)) { placeTile(m_tile, world); return; }
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) m_dragging = false;
    // the right drag turns the selected object as the stock does with its 0 1 step per pixel
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS && kField.contains(x, y)) {
            m_turning = true;
            m_turnX = x;
            m_turnY = y;
            return;
        }
        if (action == GLFW_RELEASE) { m_turning = false; return; }
    }
    // FUN 00436050 messages 0x207 and 0x208 the middle button holds the pan anchor
    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        if (action == GLFW_PRESS) {
            m_panning = true;
            m_panX = x;
            m_panY = y;
            m_panCode = 0;
            return;
        }
        m_panning = false;
        m_panCode = 0;
        return;
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void RoomCraftScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        if (key == GLFW_KEY_Q) { rotateSelected(-kYawStep); return; }
        if (key == GLFW_KEY_E) { rotateSelected(kYawStep); return; }
        if (key == GLFW_KEY_DELETE) { removeSelected(); return; }
    }
    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) { exitToLobby(); return; }
    WidgetScreen::onKey(key, action, mods);
}

void RoomCraftScreen::onAction(const std::string& action, Widget& source) {
    if (action == "lobby" || action == "channel") { exitToLobby(); return; }
    if (action == "quit") { m_app.quit(); return; }
    if (action.rfind("kind", 0) == 0) { m_kind = action[4] - '0'; m_scroll = 0; m_tile = -1; refreshTiles(); return; }
    if (action == "thumb_left") { if (m_scroll > 0) --m_scroll; return; }
    if (action == "thumb_right") { if (m_scroll + kStripTiles < static_cast<int>(m_tiles.size())) ++m_scroll; return; }
    if (action == "tip") { m_tip = !m_tip; return; }
    if (action == "del") { removeSelected(); return; }
    if (action == "info") { m_status = "the info sheet of the picked object is not in"; return; }
    if (action == "save") { save(); return; }
    if (frameAction(m_app, action)) return;
    m_status = source.id + " has no verb in this stage";
}

void RoomCraftScreen::onSession(SessionEvent event) {
    // a buy of a room object appends an owned row so the strip takes it while the stage is open
    if (event == SessionEvent::BuyOk || event == SessionEvent::InventoryChanged) {
        takeRows();
        return;
    }
    if (event == SessionEvent::RoomCraftAck) {
        takeRows();
        m_worldReady = loadField();
        refreshFieldProps();
        m_status = "0x010E ack, " + std::to_string(m_rows.size()) + " owned rows";
        return;
    }
    if (event == SessionEvent::RoomCraftSaved) {
        takeRows();
        m_worldReady = loadField();
        refreshFieldProps();
        m_status = m_app.tr("MSG_SAVE_DONE");
    }
}

// the stock leaves the editor with the lobby request 0x0012
void RoomCraftScreen::exitToLobby() {
    m_app.session().openLobby();
    m_status = "0x0012 sent";
}

}
