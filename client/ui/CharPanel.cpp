#include "CharPanel.h"

#include "MenuFrame.h"
#include "WidgetScreen.h"
#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/map_scene.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "race/RaceView.h"
#include "race/TrackData.h"
#include "screens/PendantPopup.h"

#include <bx/math.h>

#include "tools/track_scene/ghost_car.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>

namespace KnC::Client {

namespace {

// sub 4296D0 the rows start on 176 every 53 the page text sits on 230 548
constexpr float kRowY = 176.f;
constexpr float kRowStep = 53.f;
constexpr int kRowsPerPage = 7;
// the preview camera the stock kart sits low in the frame seen from its front left
constexpr float kOrbitAngle = 160.f;
constexpr float kOrbitDistance = 5.6f;
constexpr float kOrbitHeight = 1.6f;
constexpr float kOrbitLookZ = 1.2f;
constexpr float kTurnSpeed = 90.f;
// the wallpaper quad stands this far past the kart a farther one leaves the scene depth range
constexpr float kBackdropDistance = 8.f;
// the pet offsets of the stock driver place on car 0x5A32C8 and 0x5A68CC
constexpr float kPetOffsetY = 0.9f;
constexpr float kPetOffsetZ = 1.6f;
constexpr float kDegToRad = 3.14159265f / 180.f;

std::unique_ptr<ButtonWidget> artButton(AssetStore& assets, const char* id, const char* action, const char* art,
                                        float x, float y, int z) {
    auto b = std::make_unique<ButtonWidget>();
    b->type = ElementType::Button;
    b->id = id;
    b->action = action;
    b->normal = assets.texture(std::string(art) + "00.png");
    b->hover = assets.texture(std::string(art) + "01.png");
    b->pressed = assets.texture(std::string(art) + "02.png");
    const Texture* size = b->normal ? b->normal : b->hover;
    b->rect = {x, y, size ? static_cast<float>(size->width) : 24.f, size ? static_cast<float>(size->height) : 24.f};
    b->zIndex = z;
    return b;
}

// the eye and the look point of the fixed preview camera as drawOrbit places them
void previewCamera(float eye[3], float look[3]) {
    const float a = kOrbitAngle * kDegToRad;
    eye[0] = std::cos(a) * kOrbitDistance;
    eye[1] = std::sin(a) * kOrbitDistance;
    eye[2] = kOrbitLookZ + kOrbitHeight;
    look[0] = 0.f;
    look[1] = 0.f;
    look[2] = kOrbitLookZ + 0.6f;
}

// the three skin keys of a previewed kart 0 paint 1 plate 2 antenna the owned row wins
std::array<uint32_t, 3> previewSkins(App& app, const std::string& kartModel) {
    Session& session = app.session();
    const Catalog& cat = session.catalog();
    std::array<uint32_t, 3> keys{};
    for (const KartRow& row : cat.karts())
        if (row.model == kartModel) { for (size_t i = 0; i < 3; ++i) keys[i] = row.skins[i]; break; }
    if (session.profile().kartInstance >= 0)
        if (const OwnedKart* owned = cat.ownedKart(static_cast<uint32_t>(session.profile().kartInstance)))
            if (const KartRow* row = cat.kart(owned->kartKey))
                if (row->model == kartModel)
                    for (size_t i = 0; i < 3; ++i)
                        if (owned->part[i] != 0 && owned->part[i] != 0xFFFFFFFFu) keys[i] = owned->part[i];
    return keys;
}

// the model name of a part key empty when the catalogue has no row
std::string partModelOf(App& app, uint32_t key) {
    if (key == 0 || key == 0xFFFFFFFFu) return std::string();
    const PartRow* row = app.session().catalog().part(key);
    return row ? row->model : std::string();
}

// the paint part of a previewed kart the owned kart part 0 for that kart else the row skin 0
std::string previewPaint(App& app, const std::string& kartModel) {
    return partModelOf(app, previewSkins(app, kartModel)[0]);
}

// plate O NAME antenna O ANT NAMEBOX NORMAL fallback
struct PreviewLook {
    std::string plateNif;
    std::string antNif;
};

// Data Public Car Parts or Item of the model name any case empty when the file is missing
std::string carPartNif(App& app, const char* folder, const std::string& model) {
    if (model.empty()) return std::string();
    const std::string dir = app.options().gameDir + "/Data/Public/Car/" + folder;
    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ignored)) {
        if (!entry.is_regular_file(ignored)) continue;
        const std::string name = entry.path().filename().string();
        if (name.size() != model.size() + 4) continue;
        if (_stricmp(name.c_str(), (model + ".nif").c_str()) == 0) return entry.path().string();
    }
    return std::string();
}

PreviewLook previewLook(App& app, const std::string& kartModel) {
    const std::array<uint32_t, 3> keys = previewSkins(app, kartModel);
    PreviewLook look;
    std::string plate = partModelOf(app, keys[1]);
    // the default plate of car apply kart loadout when the row names none
    if (plate.empty()) plate = "NAMEBOX_NORMAL";
    look.plateNif = carPartNif(app, "Parts", plate);
    look.antNif = carPartNif(app, "Item", partModelOf(app, keys[2]));
    return look;
}

// the props of one preview view the plate the antenna and the pet live as long as its scene
struct PreviewProps {
    int plate = -1;
    int ant = -1;
    int pet = -1;
    std::string plateNif;
    std::string antNif;
    std::string petNif;
    // the two dummies of the kart body read once per kart model not once per frame
    std::string dummyKart;
    float plateLocal[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    float antLocal[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    bool hasPlate = false;
    bool hasAnt = false;
    // the seat of the driver on this kart read once per kart and driver pair
    std::string seatToken;
    float seat[3] = {0.f, 0.f, 0.f};
    bool hasSeat = false;
};

// the M FACE dds of a pet sits under Pet Facial folder not beside its body nif
std::string petFacialDir(App& app) {
    const Catalog& cat = app.session().catalog();
    const OwnedPet* worn = cat.equippedPet();
    if (!worn) return std::string();
    const PetRow* def = cat.pet(worn->petKey);
    if (!def || def->model.empty()) return std::string();
    return findEntryCi(app.options().gameDir + "/Data/Public/Pet/Facial", def->model);
}

// the asset of the worn driver the pet seat comes from its driver pos ini
std::string ownDriverAsset(App& app) {
    Session& session = app.session();
    const DriverRow* row = session.catalog().driver(session.myDriverKey());
    return row && !row->asset.empty() ? row->asset : std::string("Cosmo");
}

// pet body of the equipped 0x0104 row driver manager load driver 0x48CE97 builds it from the 0x0103 model folder
std::string equippedPetNif(App& app) {
    const Catalog& cat = app.session().catalog();
    const OwnedPet* worn = cat.equippedPet();
    if (!worn) return std::string();
    const PetRow* def = cat.pet(worn->petKey);
    if (!def || def->model.empty()) return std::string();
    const std::string folder = findEntryCi(app.options().gameDir + "/Data/Public/Pet/Body", def->model);
    if (folder.empty()) return std::string();
    return findEntryCi(folder, "body.nif");
}

std::map<const RaceView*, PreviewProps>& previewProps() {
    static std::map<const RaceView*, PreviewProps> props;
    return props;
}

// the BODYSET nifs of one driver asset from the owned row of the worn driver else the def costume
std::vector<std::string> driverPartNifs(App& app, const DriverRow& row) {
    Session& session = app.session();
    const Catalog& cat = session.catalog();
    std::array<uint32_t, 5> keys = row.costume;
    if (session.profile().characterInstance >= 0)
        if (const OwnedCharacter* mine = cat.ownedCharacter(static_cast<uint32_t>(session.profile().characterInstance)))
            if (mine->driverKey == row.key) keys = mine->accessory;
    std::vector<std::string> nifs;
    const std::string dir = app.options().gameDir + "/Data/Public/Driver/Body/High/" + row.asset + "/BODYSET/";
    std::error_code ignored;
    for (uint32_t key : keys) {
        const std::string model = partModelOf(app, key);
        if (model.empty()) continue;
        const std::string path = dir + model + ".nif";
        if (std::filesystem::exists(path, ignored)) nifs.push_back(path);
    }
    return nifs;
}

// the wallpaper png as a disk path the renderer reads its textures from files the pak copy is written out
std::string wallpaperPath(App& app) {
    const std::string onDisk = app.options().gameDir + "/Data/Public/Image/WallPaper/BackImage_00.png";
    std::error_code ignored;
    if (std::filesystem::exists(onDisk, ignored)) return onDisk;
    std::vector<uint8_t> bytes;
    if (!app.assets().readBytes("Image/WallPaper/BackImage_00.png", bytes) || bytes.empty()) return std::string();
    const std::filesystem::path out = std::filesystem::temp_directory_path(ignored) / "knc_client_wallpaper.png";
    std::ofstream file(out, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.string();
}

// one part nif as a prop of the preview scene minus one when the file does not load
int addPreviewPart(App& app, RaceView& view, const std::string& nif, const std::string& extraDir = std::string()) {
    if (nif.empty()) return -1;
    const std::string dir = std::filesystem::path(nif).parent_path().string();
    KnC::Render::PropModel model;
    std::string error;
    // an antenna binds its sphere to bones so the part loader bakes the skin at its rest pose
    if (!KnC::Tools::load_kart_part_model(nif, dir, model, error)) {
        std::printf("[charpanel] part %s failed %s\n", nif.c_str(), error.c_str());
        return -1;
    }
    KnC::Tools::resolve_textures(dir, model);
    std::printf("[charpanel] part %s %zu pieces\n", nif.c_str(), model.parts.size());
    return view.addProp(app.renderer(), model);
}

// the number plate on O NAME and the antenna on O ANT of the previewed kart body
void placePreviewParts(App& app, RaceView& view, const std::string& kartModel, const std::string& driverAsset,
                       float yawDeg) {
    PreviewProps& props = previewProps()[&view];
    const PreviewLook look = previewLook(app, kartModel);
    if (look.plateNif != props.plateNif) {
        props.plateNif = look.plateNif;
        props.plate = addPreviewPart(app, view, look.plateNif);
    }
    if (look.antNif != props.antNif) {
        props.antNif = look.antNif;
        props.ant = addPreviewPart(app, view, look.antNif);
    }
    if (props.dummyKart != kartModel) {
        props.dummyKart = kartModel;
        const std::string body = kartBodyNif(app.options().gameDir, kartModel);
        props.hasPlate = KnC::Tools::ghost_car_dummy(body, "O_NAME", props.plateLocal);
        props.hasAnt = KnC::Tools::ghost_car_dummy(body, "O_ANT", props.antLocal);
    }
    float car[16];
    bx::mtxRotateZ(car, yawDeg * kDegToRad);
    auto place = [&](int handle, const float* local, bool has) {
        if (handle < 0) return;
        float world[16];
        bx::mtxMul(world, local, car);
        view.placeProp(handle, world, has);
    };
    place(props.plate, props.plateLocal, props.hasPlate);
    place(props.ant, props.antLocal, props.hasAnt);

    // the pet of FUN 004A5ED0 at 0x4A87FE on the seat point lifted no turn no scale
    const std::string petNif = equippedPetNif(app);
    if (petNif != props.petNif) {
        props.petNif = petNif;
        props.pet = addPreviewPart(app, view, petNif, petFacialDir(app));
    }
    if (props.pet >= 0) {
        const std::string token = kartModel + "|" + driverAsset;
        if (props.seatToken != token) {
            props.seatToken = token;
            const std::string body = driverBodyNif(app.options().gameDir, driverAsset);
            props.hasSeat = !body.empty() && KnC::Tools::find_driver_seat(body, kartModel, props.seat);
        }
        float local[16];
        bx::mtxTranslate(local, props.seat[0], props.seat[1] + kPetOffsetY, props.seat[2] + kPetOffsetZ);
        float world[16];
        bx::mtxMul(world, local, car);
        view.placeProp(props.pet, world, props.hasSeat);
    }
}

}

void addCharPanelWidgets(WidgetScreen& screen, AssetStore& assets) {
    screen.add(artButton(assets, "btn_users", "users", "UserList/Lobby_Userlist_", kCharPanelTabUsers.x, kCharPanelTabUsers.y, 30));
    screen.add(artButton(assets, "btn_userinfo", "userinfo", "UserList/Lobby_Userinfo_", kCharPanelTabInfo.x, kCharPanelTabInfo.y, 30));
    screen.add(artButton(assets, "btn_rot_left", "rot_left", "CharInfo/Common_Char_Left_", 68.f, 368.f, 31));
    screen.add(artButton(assets, "btn_rot_right", "rot_right", "CharInfo/Common_Char_Right_", 360.f, 368.f, 31));
    // sub 42A260 the small blue ball at 218 516 turns the kart back to its front view
    screen.add(artButton(assets, "btn_front", "front", "CharInfo/Factory_Car_Front_", 218.f, 516.f, 31));
    screen.add(artButton(assets, "btn_pendant", "pendant", "CharInfo/Common_Char_Pendant_", 333.f, 176.f, 31));
    screen.add(artButton(assets, "btn_users_left", "users_left", "Buttons/Common_Page_Left_", 162.f, 544.f, 31));
    screen.add(artButton(assets, "btn_users_right", "users_right", "Buttons/Common_Page_Right_", 271.f, 544.f, 31));
    showCharPanelTab(screen, assets, false);
}

// the open tab wears the blue 01 art at rest as the stock init gives the User Info tab
void showCharPanelTab(WidgetScreen& screen, AssetStore& assets, bool userList) {
    for (const char* id : {"btn_rot_left", "btn_rot_right", "btn_front", "btn_pendant"})
        if (Widget* w = screen.find(id)) w->visible = !userList;
    for (const char* id : {"btn_users_left", "btn_users_right"})
        if (Widget* w = screen.find(id)) w->visible = userList;
    if (ButtonWidget* users = screen.findAs<ButtonWidget>("btn_users"))
        users->normal = assets.texture(userList ? "UserList/Lobby_Userlist_01.png" : "UserList/Lobby_Userlist_00.png");
    if (ButtonWidget* info = screen.findAs<ButtonWidget>("btn_userinfo"))
        info->normal = assets.texture(userList ? "UserList/Lobby_Userinfo_00.png" : "UserList/Lobby_Userinfo_01.png");
}

void drawCharPanel(DrawContext& ctx, AssetStore& assets, const UserListState& list, const Session* session) {
    auto sprite = [&](const std::string& path, float x, float y, float scale = 1.f) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, static_cast<float>(t->width) * scale, static_cast<float>(t->height) * scale);
    };
    if (!list.open) {
        sprite("UserList/Lobby_UserInfo_Back_00.png", 58.f, 152.f);
        // sub 429990 the slot icon at 71 179 the item icon at 71 236 the pendant button at 333 176
        sprite("CharInfo/Common_Char_Slot.png", 71.f, 179.f);
        sprite("CharInfo/Common_Char_Item.png", 71.f, 236.f);
        // 0x429C53 the worn key resolved in the 0x0119 list draws Icon base 00 at 326 172 three quarters
        if (session) {
            if (const PendantDef* worn = session->pendantDef(session->profile().pendantKey))
                sprite("Icon/" + worn->iconBase + "_00.png", 326.f, 172.f, 0.75f);
        }
        return;
    }
    sprite("UserList/Lobby_UserInfo_Back_01.png", 58.f, 152.f);
    // one 0x0132 frame carries the rows of the asked page only so the rows are drawn from zero
    for (int i = 0; i < kRowsPerPage; ++i) {
        const size_t index = static_cast<size_t>(i);
        if (index >= list.rows.size()) break;
        const UserListRow& row = list.rows[index];
        const float y = kRowY + kRowStep * static_cast<float>(i);
        char badge[48];
        std::snprintf(badge, sizeof(badge), "Icon/lv_icon_s_%03d.png", std::min(std::max(row.level, 1), 50));
        sprite(badge, 73.f, y);
        sprite("UserList/Lobby_UserInfio_Back.PNG", 106.f, y + 3.f);
        ctx.bold.draw(ctx.batch, row.name, 112.f, y + 7.f, 14.f, kInkBlack);
        if (row.pendant > 0 && row.pendant < 64) {
            char pendant[48];
            std::snprintf(pendant, sizeof(pendant), "UserList/pendant_%02d_00.png", row.pendant);
            sprite(pendant, 263.f, y + 1.f);
        } else {
            sprite("UserList/Common_Char_Pendant_ss_00.png", 263.f, y);
        }
        sprite("UserList/Common_Char_x_ss_00.png", 296.f, y);
        sprite("UserList/Common_Char_find_ss_00.png", 329.f, y);
    }
    drawAligned(ctx, ctx.bold, std::to_string(list.page + 1) + " / " + std::to_string(std::max(list.pages, 1)), 230.f, 548.f, 15.f,
                kInkBlack, Align::Centre);
}

std::string charPreviewPaint(App& app, const std::string& kartModel) { return previewPaint(app, kartModel); }

std::string charPreviewLookToken(App& app) {
    Session& session = app.session();
    const Catalog& cat = session.catalog();
    std::string token;
    if (session.profile().characterInstance >= 0)
        if (const OwnedCharacter* mine = cat.ownedCharacter(static_cast<uint32_t>(session.profile().characterInstance))) {
            token += std::to_string(mine->driverKey);
            for (uint32_t key : mine->accessory) token += ' ' + std::to_string(key);
        }
    if (session.profile().kartInstance >= 0)
        if (const OwnedKart* kart = cat.ownedKart(static_cast<uint32_t>(session.profile().kartInstance))) {
            token += " k" + std::to_string(kart->kartKey);
            for (uint32_t key : kart->part) token += ' ' + std::to_string(key);
        }
    if (const OwnedPet* worn = cat.equippedPet()) token += " p" + std::to_string(worn->petKey);
    return token;
}

bool loadCharPreviewScene(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect) {
    float eye[3], look[3];
    previewCamera(eye, look);
    const Rect canvas = {0.f, 0.f, app.canvasWidth(), app.canvasHeight()};
    return loadPreviewSceneFor(app, view, world, viewRect, eye, look, KnC::Render::kVerticalFieldOfView,
                               wallpaperPath(app), canvas);
}

bool loadPreviewSceneFor(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect, const float eye[3],
                         const float look[3], float fovDegrees, const std::string& texture, const Rect& textureRect) {
    using namespace KnC::Render;
    // every driver body loads with its worn BODYSET parts so Prince keeps his head and a hat shows
    for (const DriverRow& row : app.session().catalog().drivers())
        if (!row.asset.empty()) KnC::Tools::ghost_driver_set_parts(row.asset, driverPartNifs(app, row));
    previewProps().erase(&view);
    MapScene scene;
    scene.sun.direction[0] = -0.42f;
    scene.sun.direction[1] = 0.46f;
    scene.sun.direction[2] = -0.78f;
    scene.sun.colour = HourColour{1.f, 0.98f, 0.94f};
    scene.sun.enabled = true;
    scene.day_night = flat_day_night(HourColour{0.56f, 0.61f, 0.70f});
    scene.day_night.ambient.fill(HourColour{0.85f, 0.85f, 0.90f});
    if (!texture.empty()) {
        const bx::Vec3 e(eye[0], eye[1], eye[2]);
        const bx::Vec3 l(look[0], look[1], look[2]);
        const bx::Vec3 d = bx::normalize(bx::sub(l, e));
        const bx::Vec3 r = bx::normalize(bx::cross(d, bx::Vec3(0.f, 0.f, 1.f)));
        const bx::Vec3 u = bx::cross(r, d);
        const bx::Vec3 c = bx::add(e, bx::mul(d, kBackdropDistance));
        const float hh = kBackdropDistance * std::tan(fovDegrees * 0.5f * kDegToRad);
        const float hw = hh * (viewRect.w / viewRect.h);
        // the slice of the backdrop image that sits under the view rect on the canvas
        const float u0 = (viewRect.x - textureRect.x) / textureRect.w;
        const float u1 = (viewRect.x + viewRect.w - textureRect.x) / textureRect.w;
        const float v0 = (viewRect.y - textureRect.y) / textureRect.h;
        const float v1 = (viewRect.y + viewRect.h - textureRect.y) / textureRect.h;
        PropModel model;
        model.name = "char preview wallpaper";
        model.lit_by_map_ambient = false;
        PropPart part;
        part.texture_path = texture;
        part.has_vertex_colours = true;
        const bx::Vec3 corners[4] = {
            bx::add(bx::sub(c, bx::mul(r, hw)), bx::mul(u, hh)),
            bx::add(bx::add(c, bx::mul(r, hw)), bx::mul(u, hh)),
            bx::sub(bx::add(c, bx::mul(r, hw)), bx::mul(u, hh)),
            bx::sub(bx::sub(c, bx::mul(r, hw)), bx::mul(u, hh)),
        };
        const float uv[4][2] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
        for (int i = 0; i < 4; ++i) {
            SceneVertex v;
            v.x = corners[i].x; v.y = corners[i].y; v.z = corners[i].z;
            v.normal_x = -d.x; v.normal_y = -d.y; v.normal_z = -d.z;
            v.abgr = 0xffffffffu;
            v.u = uv[i][0]; v.v = uv[i][1];
            part.vertices.push_back(v);
        }
        // both windings so the quad shows whatever face the prop pass keeps
        const uint32_t tris[12] = {0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2};
        part.indices.assign(tris, tris + 12);
        part.bound.center[0] = c.x; part.bound.center[1] = c.y; part.bound.center[2] = c.z;
        part.bound.radius = std::sqrt(hw * hw + hh * hh);
        model.bound = part.bound;
        model.parts.push_back(std::move(part));
        scene.prop_models.push_back(std::move(model));
        PropInstance instance;
        instance.model_index = 0;
        scene.prop_instances.push_back(instance);
        reset_scene_bounds(scene);
        for (int i = 0; i < 4; ++i) {
            const float point[3] = {corners[i].x, corners[i].y, corners[i].z};
            expand_scene_bounds(point, scene);
        }
        const float origin[3] = {0.f, 0.f, 0.f};
        expand_scene_bounds(origin, scene);
    } else {
        std::printf("[charpanel] no wallpaper for the preview backdrop\n");
    }
    world = RaceWorld();
    world.scene.scene = std::move(scene);
    return view.load(app.renderer(), world);
}

// the canvas letterbox in framebuffer pixels so the preview lands on the stock view rect
void drawCharPreview(App& app, RaceView& view, int carHandle, float yawDeg, const Rect& viewRect) {
    float px[4];
    app.canvasToPixels(viewRect.x, viewRect.y, viewRect.w, viewRect.h, px);
    KnC::Render::ViewportRect box;
    box.x = static_cast<uint16_t>(px[0]);
    box.y = static_cast<uint16_t>(px[1]);
    box.width = static_cast<uint16_t>(px[2]);
    box.height = static_cast<uint16_t>(px[3]);
    const KnC::Render::ViewportRect& now = app.renderer().viewport();
    if (now.x != box.x || now.y != box.y || now.width != box.width || now.height != box.height)
        app.renderer().set_viewport(box);
    if (carHandle >= 0) {
        // the BodyColor paint of the kart Car Body High model sub 0x49123D
        const std::string kartModel = view.kartModelOf(carHandle);
        view.setCarPaint(app.renderer(), carHandle, previewPaint(app, kartModel));
        CarPose pose;
        pose.yawDeg = yawDeg;
        view.setPose(carHandle, pose, 0.f, 0.f);
        placePreviewParts(app, view, kartModel, ownDriverAsset(app), yawDeg);
    }
    CarPose target;
    target.z = kOrbitLookZ;
    view.drawOrbit(app.renderer(), target, kOrbitAngle, kOrbitDistance, kOrbitHeight, 1.f / 25.f);
}

float turnCharPreview(WidgetScreen& screen, float yawDeg, float dt) {
    const ButtonWidget* left = screen.findAs<ButtonWidget>("btn_rot_left");
    const ButtonWidget* right = screen.findAs<ButtonWidget>("btn_rot_right");
    if (left && left->visible && left->state == ButtonWidget::State::Pressed) yawDeg -= kTurnSpeed * dt;
    if (right && right->visible && right->state == ButtonWidget::State::Pressed) yawDeg += kTurnSpeed * dt;
    while (yawDeg < 0.f) yawDeg += 360.f;
    while (yawDeg >= 360.f) yawDeg -= 360.f;
    return yawDeg;
}

void hookUserListTap(App& app, UserListState* state) {
    app.setFrameTap([state](uint16_t op, Packet& pkt) {
        if (op != 0x0132 || pkt.remaining() < 296 || !state) return;
        state->page = static_cast<int>(pkt.readUInt32());
        state->pages = static_cast<int>(pkt.readUInt32());
        pkt.readUInt32();
        std::vector<UserListRow> rows;
        for (int i = 0; i < 7; ++i) {
            const std::vector<uint8_t> rec = pkt.readBytes(0x28);
            if (rec.size() < 0x28) break;
            UserListRow row;
            row.playerId = static_cast<uint32_t>(rec[0]) | (static_cast<uint32_t>(rec[1]) << 8) | (static_cast<uint32_t>(rec[2]) << 16) | (static_cast<uint32_t>(rec[3]) << 24);
            std::u16string name;
            for (size_t k = 0; k < 14; ++k) {
                const char16_t c = static_cast<char16_t>(rec[4 + 2 * k] | (rec[5 + 2 * k] << 8));
                if (c == 0) break;
                name.push_back(c);
            }
            row.name = u16ToUtf8(name);
            row.level = static_cast<int>(rec[0x20] | (rec[0x21] << 8));
            row.pendant = static_cast<int32_t>(static_cast<uint32_t>(rec[0x24]) | (static_cast<uint32_t>(rec[0x25]) << 8) | (static_cast<uint32_t>(rec[0x26]) << 16) | (static_cast<uint32_t>(rec[0x27]) << 24));
            rows.push_back(row);
        }
        const uint32_t count = pkt.remaining() >= 4 ? pkt.readUInt32() : 0;
        if (count < rows.size()) rows.resize(count);
        state->rows = rows;
    });
}

void requestUserListPage(App& app, UserListState& state, int page) {
    state.page = std::max(page, 0);
    Packet req = Packet::fromCmdFull(0x0132);
    req.writeUInt32(static_cast<uint32_t>(state.page));
    app.session().send(req);
}

bool charPanelAction(WidgetScreen& screen, App& app, UserListState& state, const std::string& action, float* yawDeg) {
    if (action == "users") { state.open = true; showCharPanelTab(screen, app.assets(), true); requestUserListPage(app, state, 0); return true; }
    if (action == "userinfo") { state.open = false; showCharPanelTab(screen, app.assets(), false); return true; }
    if (action == "users_left") { if (state.page > 0) requestUserListPage(app, state, state.page - 1); return true; }
    if (action == "users_right") { if (state.page + 1 < state.pages) requestUserListPage(app, state, state.page + 1); return true; }
    if (action == "front") { if (yawDeg) *yawDeg = 0.f; return true; }
    // 0x4295A0 the pendant button opens the box through 0x46F300 only while User Info is open
    if (action == "pendant") {
        if (!state.open) app.pushScreen(std::make_unique<PendantPopup>(app));
        return true;
    }
    return action == "rot_left" || action == "rot_right";
}

}
