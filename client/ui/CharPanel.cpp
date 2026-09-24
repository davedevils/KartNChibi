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
// sub 4A5E00 heading 286 of the lobby panel and the licence 0x44DF00 turns it by 90 minus heading
constexpr float kPreviewHeading = 286.f;
// sub 4A5ED0 0x4A89C2 the eye 28 out from 0 0 2 lifted 0 3 looks at the origin
constexpr float kPreviewDistance = 28.f;
constexpr float kPreviewTargetZ = 2.f;
constexpr float kPreviewLift = 0.3f;
// 0x4A894C lens 0 2443 rad vertical the horizontal takes the window aspect term of base 1
constexpr float kPreviewFieldRadians = 0.24434609f;
constexpr float kStockAspectPivot = 1.778f;
constexpr float kPreviewAspectBase = 1.f;
// sub 4A4E40 the kart stands 0 3 along x and its row stat 13 minus 2 high
constexpr float kPreviewKartX = 0.3f;
constexpr float kPreviewKartDrop = 2.f;
constexpr float kTurnSpeed = 90.f;
// the wallpaper quad stands past the kart inside the far plane of the preview scene
constexpr float kBackdropDistance = 50.f;
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

void previewCamera(float eye[3], float look[3]) {
    const float a = (90.f - kPreviewHeading) * kDegToRad;
    eye[0] = std::cos(a) * kPreviewDistance;
    eye[1] = std::sin(a) * kPreviewDistance;
    eye[2] = kPreviewTargetZ + kPreviewLift;
    look[0] = 0.f;
    look[1] = 0.f;
    look[2] = 0.f;
}

// the stock aspect term reads the device size ours reads the canvas as the window shows it
float previewHorizontalField(App& app) {
    float px[4];
    app.canvasToPixels(0.f, 0.f, app.canvasWidth(), app.canvasHeight(), px);
    const float aspect = px[3] > 0.f ? px[2] / px[3] : 4.f / 3.f;
    return kPreviewFieldRadians * ((aspect - kStockAspectPivot) * 0.5f + kPreviewAspectBase);
}

// row stat 13 of the previewed kart model zero when the catalogue has no row
float previewKartStat13(App& app, const std::string& kartModel) {
    const std::string chassis = KnC::Tools::ghost_kart_chassis(kartModel);
    for (const KartRow& row : app.session().catalog().karts())
        if (row.model == chassis) return row.stats[13];
    return 0.f;
}

// the stock kart matrix D3DX RotationZ of plus yaw then the lift bx turns the other way
CarPose previewKartPose(App& app, const std::string& kartModel, float yawDeg) {
    CarPose pose;
    pose.yawDeg = -yawDeg;
    pose.x = kPreviewKartX;
    pose.z = previewKartStat13(app, kartModel) - kPreviewKartDrop;
    return pose;
}

void previewKartMatrix(const CarPose& pose, float out[16]) {
    float rotate[16], translate[16];
    bx::mtxRotateZ(rotate, pose.yawDeg * kDegToRad);
    bx::mtxTranslate(translate, pose.x, pose.y, pose.z);
    bx::mtxMul(out, rotate, translate);
}

// the three skin keys of a previewed kart 0 paint 1 plate 2 antenna the owned row wins
std::array<uint32_t, 3> previewSkins(App& app, const std::string& kartModel) {
    Session& session = app.session();
    const Catalog& cat = session.catalog();
    const std::string chassis = KnC::Tools::ghost_kart_chassis(kartModel);
    std::array<uint32_t, 3> keys{};
    for (const KartRow& row : cat.karts())
        if (row.model == chassis) { for (size_t i = 0; i < 3; ++i) keys[i] = row.skins[i]; break; }
    if (session.profile().kartInstance >= 0)
        if (const OwnedKart* owned = cat.ownedKart(static_cast<uint32_t>(session.profile().kartInstance)))
            if (const KartRow* row = cat.kart(owned->kartKey))
                if (row->model == chassis)
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

// the props of one preview view the plate and the antenna live as long as its scene
struct PreviewProps {
    int plate = -1;
    int ant = -1;
    std::string plateNif;
    std::string antNif;
    std::string petNif;
    // the car the pet rides a preview that swaps its kart gets a new handle
    int petCar = -1;
    // the two dummies of the kart body read once per kart model not once per frame
    std::string dummyKart;
    float plateLocal[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    float antLocal[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    bool hasPlate = false;
    bool hasAnt = false;
};

// the part a shop tile tries on one preview view kept apart from the props its scene drops
std::map<const RaceView*, PreviewTryOn>& previewTryOns() {
    static std::map<const RaceView*, PreviewTryOn> tryOns;
    return tryOns;
}

const PreviewTryOn& tryOnOf(const RaceView& view) {
    static const PreviewTryOn none;
    const auto found = previewTryOns().find(&view);
    return found == previewTryOns().end() ? none : found->second;
}

// the pet a preview shows the tried key else the equipped row
const PetRow* previewPet(App& app, const RaceView& view) {
    const Catalog& cat = app.session().catalog();
    if (tryOnOf(view).petKey != 0) return cat.pet(tryOnOf(view).petKey);
    const OwnedPet* worn = cat.equippedPet();
    return worn ? cat.pet(worn->petKey) : nullptr;
}

// the M FACE dds of a pet sits under Pet Facial folder not beside its body nif
std::string petFacialDir(App& app, const PetRow* def) {
    if (!def || def->model.empty()) return std::string();
    return findEntryCi(app.options().gameDir + "/Data/Public/Pet/Facial", def->model);
}

// pet body of a 0x0103 row driver manager load driver 0x48CE97 builds it from the model folder
std::string petBodyNif(App& app, const PetRow* def) {
    if (!def || def->model.empty()) return std::string();
    const std::string folder = findEntryCi(app.options().gameDir + "/Data/Public/Pet/Body", def->model);
    if (folder.empty()) return std::string();
    return findEntryCi(folder, "body.nif");
}

std::map<const RaceView*, PreviewProps>& previewProps() {
    static std::map<const RaceView*, PreviewProps> props;
    return props;
}

// the worn look of one driver asset the owned row of the worn driver else the def costume
std::vector<KnC::Tools::GhostDriverPart> driverPartNifs(App& app, const DriverRow& row) {
    return driverParts(app, row.asset, wornCostume(app, row.key));
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
void placePreviewParts(App& app, RaceView& view, int carHandle, const std::string& kartModel, const float car[16]) {
    PreviewProps& props = previewProps()[&view];
    PreviewLook look = previewLook(app, kartModel);
    const PreviewTryOn& tried = tryOnOf(view);
    if (!tried.plate.empty()) look.plateNif = carPartNif(app, "Parts", tried.plate);
    if (!tried.antenna.empty()) look.antNif = carPartNif(app, "Item", tried.antenna);
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
    // the plate and antenna hang under O NAME and O ANT so they follow the body bounce
    auto place = [&](int handle, const char* node, const float* local, bool has) {
        if (handle < 0) return;
        float world[16];
        if (!view.carNodeWorld(app.renderer(), carHandle, node, world)) bx::mtxMul(world, local, car);
        view.placeProp(handle, world, has);
    };
    place(props.plate, "O_NAME", props.plateLocal, props.hasPlate);
    place(props.ant, "O_ANT", props.antLocal, props.hasAnt);

    // FUN 004A5ED0 loads the pet with its kfm sub 4A51B0 hovers it by the seat a shop tile tries one
    const PetRow* pet = previewPet(app, view);
    const std::string petNif = petBodyNif(app, pet);
    if (petNif != props.petNif || carHandle != props.petCar) {
        props.petNif = petNif;
        props.petCar = carHandle;
        view.setCarPet(app.renderer(), carHandle, petNif, petFacialDir(app, pet), PetHoverKind::Preview);
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

void setCharPreviewTryOn(const RaceView& view, const PreviewTryOn& tryOn) { previewTryOns()[&view] = tryOn; }

std::array<uint32_t, 5> wornCostume(App& app, uint32_t driverKey) {
    Session& session = app.session();
    const Catalog& cat = session.catalog();
    std::array<uint32_t, 5> keys{};
    if (const DriverRow* row = cat.driver(driverKey)) keys = row->costume;
    if (session.profile().characterInstance >= 0)
        if (const OwnedCharacter* mine = cat.ownedCharacter(static_cast<uint32_t>(session.profile().characterInstance)))
            if (mine->driverKey == driverKey) keys = mine->accessory;
    return keys;
}

KartLook kartLook(App& app, uint32_t kartKey, const std::array<uint32_t, 3>& parts) {
    std::array<uint32_t, 3> keys{};
    if (const KartRow* row = app.session().catalog().kart(kartKey))
        for (size_t i = 0; i < keys.size(); ++i) keys[i] = row->skins[i];
    for (size_t i = 0; i < keys.size(); ++i)
        if (parts[i] != 0 && parts[i] != 0xFFFFFFFFu) keys[i] = parts[i];
    KartLook look;
    look.paint = partModelOf(app, keys[0]);
    look.plate = partModelOf(app, keys[1]);
    if (look.plate.empty()) look.plate = "NAMEBOX_NORMAL";
    look.antenna = partModelOf(app, keys[2]);
    return look;
}

std::string kartViewModel(App& app, uint32_t kartKey, const std::array<uint32_t, 15>& customCar) {
    const Catalog& cat = app.session().catalog();
    const KartRow* row = cat.kart(kartKey);
    const std::string model = row && !row->model.empty() ? row->model : std::string("Basic_1");
    if (!row || row->modelScheme != 1) return model;
    KnC::Tools::GhostFactoryCar car;
    car.chassis = model;
    for (size_t slot = 0; slot < car.parts.size(); ++slot) {
        const CarCraftPartDef* def = cat.carCraftPart(customCar[1 + slot * 2]);
        if (!def || def->model.empty()) continue;
        car.parts[slot].model = def->model;
        car.parts[slot].grade = static_cast<int32_t>(customCar[2 + slot * 2]);
    }
    return KnC::Tools::ghost_factory_token(car);
}

std::string ownedKartViewModel(App& app, uint32_t kartInstance) {
    const Catalog& cat = app.session().catalog();
    const OwnedKart* owned = cat.ownedKart(kartInstance);
    std::array<uint32_t, 15> block{};
    for (const CarCraftPreset& preset : cat.carCraftPresets()) {
        if (preset.kartInstance != kartInstance) continue;
        // the preset holds part instances the block the server sends holds their def keys and grades
        for (size_t slot = 0; slot < preset.slot.size(); ++slot) {
            const CarCraftPartInstance* part = cat.carCraftInstance(preset.slot[slot]);
            if (!part) continue;
            block[1 + slot * 2] = part->partKey;
            block[2 + slot * 2] = static_cast<uint32_t>(part->grade);
        }
        break;
    }
    return kartViewModel(app, owned ? owned->kartKey : 0, block);
}

std::vector<KnC::Tools::GhostDriverPart> driverParts(App& app, const std::string& asset,
                                                     const std::array<uint32_t, 5>& keys) {
    std::vector<KnC::Tools::GhostDriverPart> parts;
    const std::string dir = findEntryCi(findEntryCi(app.options().gameDir + "/Data/Public/Driver/Body/High", asset), "BODYSET");
    for (size_t i = 0; i < keys.size(); ++i) {
        const std::string model = partModelOf(app, keys[i]);
        const std::string nif = model.empty() || dir.empty() ? std::string() : findEntryCi(dir, model + ".nif");
        // sub 48C9C0 slot 2 O BODY to 6 O BACK in the order of the five keys
        if (!nif.empty()) parts.push_back({static_cast<int>(i) + 2, nif});
    }
    return parts;
}

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
    return loadCharPreviewSceneFor(app, view, world, viewRect, wallpaperPath(app),
                                   Rect{0.f, 0.f, app.canvasWidth(), app.canvasHeight()});
}

bool loadCharPreviewSceneFor(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect,
                             const std::string& texture, const Rect& textureRect) {
    float eye[3], look[3];
    previewCamera(eye, look);
    return loadPreviewSceneFor(app, view, world, viewRect, eye, look, kPreviewFieldRadians / kDegToRad, texture,
                               textureRect, previewHorizontalField(app) / kDegToRad);
}

bool loadPreviewSceneFor(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect, const float eye[3],
                         const float look[3], float fovDegrees, const std::string& texture, const Rect& textureRect,
                         float horizontalDegrees) {
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
        // a lens with its own horizontal field spans that angle across the rect not the rect aspect
        const float hw = horizontalDegrees > 0.f ? kBackdropDistance * std::tan(horizontalDegrees * 0.5f * kDegToRad)
                                                 : hh * (viewRect.w / viewRect.h);
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
        const std::string& tried = tryOnOf(view).paint;
        view.setCarPaint(app.renderer(), carHandle, tried.empty() ? previewPaint(app, kartModel) : tried);
        const CarPose pose = previewKartPose(app, kartModel, yawDeg);
        view.setPose(carHandle, pose, 0.f, 0.f);
        float car[16];
        previewKartMatrix(pose, car);
        placePreviewParts(app, view, carHandle, kartModel, car);
    }
    float eye[3], look[3];
    previewCamera(eye, look);
    view.drawFixed(app.renderer(), eye, look, kPreviewFieldRadians, 1.f / 25.f, previewHorizontalField(app));
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
