#include "CarCraftScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/map_scene.h"
#include "engine/render/nif_prop_model.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "race/TrackData.h"
#include "screens/GarageScreen.h"
#include "screens/ShopCommon.h"
#include "screens/ShopScreen.h"
#include "tools/track_scene/ghost_car.h"
#include "ui/CharPanel.h"
#include "ui/MenuFrame.h"

#include <GLFW/glfw3.h>
#include <bx/math.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace KnC::Client {

namespace {

// sub 430EF0 top at 25 50 back at 37 88 buttons at their init spots
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;

struct CraftButton {
    const char* id;
    const char* action;
    const char* art;
    float x;
    float y;
};

const CraftButton kButtons[] = {
    {"btn_save", "save", "Buttons/Common_Room_Save_", 845.f, 669.f},
    {"btn_list_up", "list_up", "CarFactory/Factory_Car_List_Up_", 56.f, 408.f},
    {"btn_list_down", "list_down", "CarFactory/Factory_Car_List_Down_", 56.f, 627.f},
    {"btn_slot_right", "slot_right", "CarFactory/Factory_Car_Slot_Right_", 936.f, 97.f},
    {"btn_slot_left", "slot_left", "CarFactory/Factory_Car_Slot_Left_", 46.f, 97.f},
    {"btn_car_right", "car_right", "CarFactory/Common_Car_Right_", 711.f, 667.f},
    {"btn_car_left", "car_left", "CarFactory/Common_Car_Left_", 627.f, 667.f},
    {"btn_car_front", "car_front", "CarFactory/Factory_Car_Front_", 666.f, 669.f},
    {"btn_install", "install", "CarFactory/Factory_Car_Install_", 101.f, 674.f},
    {"btn_remove", "remove", "CarFactory/Factory_Car_Remove_", 215.f, 674.f},
    {"btn_machine", "machine", "CarFactory/Factory_Car_Machine_Name_", 399.f, 228.f},
};

// part tabs from init in their slot order picked one rests on its 01 art
struct PartTab {
    const char* art;
    float x;
    float y;
};

const PartTab kPartTabs[8] = {
    {"CarFactory/Factory_Car_Parts_Chassis_", 61.f, 237.f},
    {"CarFactory/Factory_Car_Parts_Cover_", 139.f, 237.f},
    {"CarFactory/Factory_Car_Parts_Booster_", 295.f, 237.f},
    {"CarFactory/Factory_Car_Parts_Tire_", 217.f, 237.f},
    {"CarFactory/Factory_Car_Parts_Frontfender_", 139.f, 316.f},
    {"CarFactory/Factory_Car_Parts_Rearfender_", 217.f, 316.f},
    {"CarFactory/Factory_Car_Parts_Bumper_", 61.f, 316.f},
    {"CarFactory/Factory_Car_Parts_Spoiler_", 295.f, 316.f},
};

// preset strip four tiles of 212 from 87 107 name at plus 122 56
constexpr float kSlotX = 87.f;
constexpr float kSlotY = 107.f;
constexpr float kSlotStep = 212.f;
constexpr float kSlotW = 205.f;
constexpr float kSlotH = 95.f;
constexpr int kSlotsShown = 4;
// part list three rows of 64 from 435 box at 58 name at 134 plus 12
constexpr float kListX = 58.f;
constexpr float kListY = 435.f;
constexpr float kListStep = 64.f;
constexpr int kListRows = 3;
// four bars at 638 237 and 823 237 lower pair 27 under them run is 131 px
constexpr float kBarLeftX = 638.f;
constexpr float kBarRightX = 823.f;
constexpr float kBarTopY = 237.f;
constexpr float kBarLowY = 264.f;
constexpr float kBarW = 131.f;
constexpr float kBarH = 12.f;
// sub 430EF0 SetRect factory car shows on 400 310 of 562 by 340
constexpr Rect kPreviewRect = {400.f, 310.f, 562.f, 340.f};
// stage camera eye minus 4 0 1 3 look at origin field 1 0 radians
constexpr float kPreviewEye[3] = {-4.f, 0.1f, 3.f};
constexpr float kPreviewLook[3] = {0.f, 0.f, 0.f};
constexpr float kPreviewField = 1.f;
// 0x4310A2 the lens aspect term is 1 6 or 1 92 while wide mode is on
constexpr float kPreviewAspect = 1.6f;
constexpr float kPreviewAspectWide = 1.92f;
constexpr float kTurnSpeed = 90.f;
constexpr float kDegToRad = 3.14159265f / 180.f;

// seven slot folders from FactoryCar tree in 0x0108 category order
struct CraftSlotArt {
    const char* folder;
    const char* file;
    int count;
};
const CraftSlotArt kSlotArt[7] = {
    {"COVER", "COVER", 1},       {"BOOSTER", "BOOSTER", 1}, {"TIRES", "WHEEL", 4}, {"F_FENDER", "F_FENDER", 2},
    {"R_FENDER", "R_FENDER", 2}, {"BUMPER", "BUMPER", 1},   {"WING", "WING", 1},
};

// back art as a disk path renderer reads textures from files pak copy written out
std::string backArtPath(App& app) {
    std::error_code ignored;
    for (const char* lang : {"Eng", "Public"}) {
        const std::string onDisk = app.options().gameDir + "/Data/" + lang + "/Image/CarFactory/Factory_Car_Back.png";
        if (std::filesystem::exists(onDisk, ignored)) return onDisk;
    }
    std::vector<uint8_t> bytes;
    if (!app.assets().readBytes("Image/CarFactory/Factory_Car_Back.png", bytes) || bytes.empty()) return std::string();
    const std::filesystem::path out = std::filesystem::temp_directory_path(ignored) / "knc_client_factory_back.png";
    std::ofstream file(out, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.string();
}

// grade words of FUN 0042FE20 basic under five unique under twenty epic up to sixty four
const char* gradeKey(int grade) {
    if (grade < 5) return "CAR_PART_BASIC";
    if (grade < 20) return "CAR_PART_UNIQUE";
    if (grade < 65) return "CAR_PART_EPIC";
    return "CAR_PART_LEGEND";
}

uint32_t rawU32(const std::array<uint8_t, 0x84>& raw, size_t at) {
    return static_cast<uint32_t>(raw[at]) | (static_cast<uint32_t>(raw[at + 1]) << 8) |
           (static_cast<uint32_t>(raw[at + 2]) << 16) | (static_cast<uint32_t>(raw[at + 3]) << 24);
}

// FUN 00451C30 day number 365 times tm year plus tm yday minus 365
int32_t dayNumberToday() {
    const std::time_t now = std::time(nullptr);
    std::tm parts{};
#if defined(_WIN32)
    localtime_s(&parts, &now);
#else
    localtime_r(&now, &parts);
#endif
    return 365 * parts.tm_year + parts.tm_yday - 365;
}

// sub 42FE20 rows from 435 every 64 the magnifier at 340 plus 36 the period words at 146 plus 38
constexpr float kRowTextX = 134.f;
constexpr float kRowPeriodX = 146.f;
constexpr float kRowInfoX = 340.f;
constexpr float kRowDurX = 126.f;
constexpr float kRowFontPx = 15.f;
// sub 42EFE0 the slot name at slot x plus 124 y 165 centred the plate name at 488 244
constexpr float kSlotNameDX = 124.f;
constexpr float kSlotNameY = 165.f;
constexpr float kPlateNameX = 488.f;
constexpr float kPlateNameY = 244.f;

}

// 0x20 config order is kart cover tires booster bumper front fender rear fender wing
int CarCraftScreen::slotOfCategory(int category) {
    static const int kSlot[7] = {0, 2, 1, 4, 5, 3, 6};
    return category >= 0 && category < 7 ? kSlot[category] : -1;
}

// tab id equals slot type plus one so chassis tab has no category
int CarCraftScreen::tabCategory() const { return m_part == 0 ? -1 : m_part - 1; }

const CarCraftPreset* CarCraftScreen::preset() const {
    const std::vector<CarCraftPreset>& rows = m_app.session().catalog().carCraftPresets();
    if (m_preset < 0 || m_preset >= static_cast<int>(rows.size())) return nullptr;
    return &rows[static_cast<size_t>(m_preset)];
}

void CarCraftScreen::enter() {
    AssetStore& assets = m_app.assets();
    addMenuFrame(*this, assets, FrameMode::Full, "carcraft");
    for (const CraftButton& c : kButtons) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = c.id;
        b->action = c.action;
        b->normal = assets.texture(std::string(c.art) + "00.png");
        b->hover = assets.texture(std::string(c.art) + "01.png");
        b->pressed = assets.texture(std::string(c.art) + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {c.x, c.y, size ? static_cast<float>(size->width) : 60.f, size ? static_cast<float>(size->height) : 24.f};
        b->zIndex = 30;
        add(std::move(b));
    }
    for (int i = 0; i < 8; ++i) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = "btn_part" + std::to_string(i);
        b->action = "part" + std::to_string(i);
        b->normal = assets.texture(std::string(kPartTabs[i].art) + "00.png");
        b->hover = assets.texture(std::string(kPartTabs[i].art) + "01.png");
        b->pressed = assets.texture(std::string(kPartTabs[i].art) + "02.png");
        const Texture* size = b->normal ? b->normal : b->hover;
        b->rect = {kPartTabs[i].x, kPartTabs[i].y, size ? static_cast<float>(size->width) : 70.f, size ? static_cast<float>(size->height) : 70.f};
        b->zIndex = 30;
        add(std::move(b));
    }
    m_part = 0;
    m_scroll = 0;
    m_presetScroll = 0;
    m_selected = -1;
    m_dirty = false;
    m_renaming = false;
    m_time = 0.f;
    m_captured = false;
    m_status.clear();
    // stock stage opens on 0x010A request presets defs and instances come first
    if (!m_opened) {
        m_opened = true;
        m_app.session().openCarCraft();
    }
    selectPreset(0);
}

// picked preset copies its config into stage 3D car and bars follow it
void CarCraftScreen::selectPreset(int index) {
    const std::vector<CarCraftPreset>& rows = m_app.session().catalog().carCraftPresets();
    if (rows.empty()) { m_preset = 0; m_config = CarConfig(); refreshRows(); return; }
    m_preset = std::min(std::max(index, 0), static_cast<int>(rows.size()) - 1);
    const CarCraftPreset& row = rows[static_cast<size_t>(m_preset)];
    m_config.kartInstance = row.kartInstance;
    m_config.slot = row.slot;
    m_selected = -1;
    m_scroll = 0;
    m_dirty = false;
    refreshRows();
}

// FUN 0042FE20 chassis tab lists owned factory karts others one row per owned part instance of that slot
void CarCraftScreen::refreshRows() {
    m_rows.clear();
    const Catalog& cat = m_app.session().catalog();
    const std::vector<CarCraftPreset>& presets = cat.carCraftPresets();
    if (m_part == 0) {
        for (const OwnedKart& owned : cat.ownedKarts()) {
            const KartRow* def = cat.kart(owned.kartKey);
            // model scheme 1 is the factory chassis a catalogue kart never shows here
            if (!def || def->modelScheme != 1) continue;
            ListRow row;
            row.instance = owned.instance;
            row.key = owned.kartKey;
            row.label = m_app.tr(def->nameKey);
            row.icon = kartIcon(*def);
            row.installed = m_config.kartInstance == owned.instance;
            row.inUse = row.installed;
            for (size_t i = 0; i < presets.size(); ++i)
                if (static_cast<int>(i) != m_preset && presets[i].kartInstance == owned.instance) row.inUse = true;
            row.periodType = owned.expiryKind;
            row.periodValue = owned.durability;
            row.active = owned.active != 0;
            m_rows.push_back(row);
        }
    } else {
        const int category = tabCategory();
        const int slot = slotOfCategory(category);
        const uint32_t onSlot = slot >= 0 ? m_config.slot[static_cast<size_t>(slot)] : 0;
        const CarCraftPartInstance* installed = onSlot ? cat.carCraftInstance(onSlot) : nullptr;
        for (const CarCraftPartInstance& owned : cat.carCraftInstances()) {
            const CarCraftPartDef* def = cat.carCraftPart(owned.partKey);
            if (!def || static_cast<int>(def->category) != category) continue;
            ListRow row;
            row.instance = owned.instance;
            row.key = owned.partKey;
            row.label = m_app.tr(def->nameKey) + " - " + m_app.tr(gradeKey(owned.grade));
            row.icon = carCraftIcon(def->model, category);
            row.grade = owned.grade;
            row.installed = onSlot == owned.instance;
            row.periodType = rawU32(owned.raw, 0x14);
            row.periodValue = static_cast<int32_t>(rawU32(owned.raw, 0x18));
            row.active = rawU32(owned.raw, 0x1C) != 0;
            row.count = owned.refCount;
            row.inUse = (installed && installed->partKey == owned.partKey) || (owned.refCount > 0 && row.active);
            m_rows.push_back(row);
        }
    }
    if (m_selected >= static_cast<int>(m_rows.size())) m_selected = -1;
    if (m_scroll > std::max(0, static_cast<int>(m_rows.size()) - kListRows)) m_scroll = std::max(0, static_cast<int>(m_rows.size()) - kListRows);
}

// FUN 0042F6C0 old part loses one refcount new one gains one config takes its instance
void CarCraftScreen::install() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_rows.size())) { m_status = "pick a row first"; return; }
    const ListRow& row = m_rows[static_cast<size_t>(m_selected)];
    if (m_part == 0) {
        if (m_config.kartInstance == row.instance) { m_status = "that chassis is already on"; return; }
        m_config.kartInstance = row.instance;
        m_dirty = true;
        m_status = "chassis " + row.label + " on the preset";
        refreshRows();
        return;
    }
    const int slot = slotOfCategory(tabCategory());
    if (slot < 0) return;
    // FUN 0042F6C0 a part goes only on a slot with an owned chassis and only a live row
    if (!m_app.session().catalog().ownedKart(m_config.kartInstance)) { m_status = "this slot holds no chassis"; return; }
    if (!row.active) { m_status = m_app.tr("UNIT_EXPIRED"); return; }
    std::vector<CarCraftPartInstance>& parts = m_app.session().catalogForEdit().carCraftInstances();
    const uint32_t previous = m_config.slot[static_cast<size_t>(slot)];
    for (CarCraftPartInstance& p : parts) {
        if (p.instance != previous) continue;
        if (p.partKey == row.key) { m_status = "that part is already on"; return; }
        if (--p.refCount < 0) p.refCount = 0;
    }
    m_config.slot[static_cast<size_t>(slot)] = row.instance;
    for (CarCraftPartInstance& p : parts) if (p.instance == row.instance) ++p.refCount;
    m_dirty = true;
    m_status = "installed " + row.label + " in slot " + std::to_string(slot);
    refreshRows();
}

// FUN 00432B20 case 9 slot goes to zero part loses one refcount chassis cannot go
void CarCraftScreen::removePart() {
    if (m_part == 0) { m_status = m_app.tr("MSG_UNSUPPORT"); return; }
    const int slot = slotOfCategory(tabCategory());
    if (slot < 0) return;
    const uint32_t installed = m_config.slot[static_cast<size_t>(slot)];
    if (installed == 0) { m_status = "that slot is empty"; return; }
    std::vector<CarCraftPartInstance>& parts = m_app.session().catalogForEdit().carCraftInstances();
    for (CarCraftPartInstance& p : parts) if (p.instance == installed && --p.refCount < 0) p.refCount = 0;
    m_config.slot[static_cast<size_t>(slot)] = 0;
    m_dirty = true;
    m_status = "slot " + std::to_string(slot) + " cleared";
    refreshRows();
}

// FUN 0042F3E0 kart and tires must resolve then whole local part list goes out
void CarCraftScreen::save() {
    const CarCraftPreset* row = preset();
    if (!row) { m_status = "no preset row landed"; return; }
    const Catalog& cat = m_app.session().catalog();
    // tires sit in config slot for category two not third dword
    const uint32_t tires = m_config.slot[static_cast<size_t>(slotOfCategory(2))];
    const bool kartOk = cat.ownedKart(m_config.kartInstance) != nullptr;
    const bool tiresOk = tires != 0 && cat.carCraftInstance(tires) != nullptr;
    if (!kartOk || !tiresOk) { m_status = m_app.tr("MSG_UNSUPPORT"); return; }
    const std::vector<CarCraftPartInstance>& parts = cat.carCraftInstances();
    if (parts.empty() && !m_dirty) { m_status = m_app.tr("MSG_SAVE_DONE"); return; }
    m_app.session().saveCarCraft(row->presetId, m_config, parts);
    m_status = "0x010B sent for preset " + std::to_string(row->presetId);
}

// FUN 0045E110 mode 1 the shop item box with the part name art text and prices its Buy sends 0x00B7
void CarCraftScreen::openPartInfo(const ListRow& row) {
    if (m_part == 0) { m_status = "the chassis info box is not in"; return; }
    const Catalog& cat = m_app.session().catalog();
    const CarCraftPartDef* def = cat.carCraftPart(row.key);
    if (!def) return;
    // 0x4328EF a part def that is not on sale opens the box in mode 0 which ours does not draw
    if (def->enabled == 0) { m_status = "that part is not on sale"; return; }
    ShopTile tile;
    tile.category = static_cast<uint32_t>(BuyCategory::CarCraft);
    tile.baseKey = def->key;
    tile.label = m_app.tr(def->nameKey);
    tile.icon = carCraftIcon(def->model, static_cast<int>(def->category));
    tile.bigIcon = carCraftIcon(def->model, static_cast<int>(def->category), true);
    tile.descKey = def->descKey;
    tile.quotes = quotesOf(cat, def->prices);
    tile.owned = true;
    m_app.pushScreen(std::make_unique<ShopItemPopup>(m_app, tile, true));
}

void CarCraftScreen::beginRename() {
    const CarCraftPreset* row = preset();
    // button 10 of sub 432B20 answers only on a built slot
    if (!row || row->slotState != 1) return;
    m_renaming = true;
    m_rename = row->name;
    m_status = "type a name of nine characters then Enter";
}

void CarCraftScreen::submitRename() {
    const CarCraftPreset* row = preset();
    m_renaming = false;
    if (!row || m_rename.empty()) return;
    m_app.session().renameCarCraftPreset(row->presetId, m_rename);
    m_status = "0x0114 sent with " + m_rename;
}

std::string CarCraftScreen::periodText(uint32_t type, int32_t value) const {
    char text[64];
    switch (type) {
    case 0: return m_app.tr("UNIT_PERMANENT");
    case 1: std::snprintf(text, sizeof(text), "%4d%s", value - dayNumberToday(), m_app.tr("UNIT_DAYS").c_str()); return text;
    case 2: std::snprintf(text, sizeof(text), "%4d%s", value, m_app.tr("UNIT_TIMES").c_str()); return text;
    case 3: std::snprintf(text, sizeof(text), "%4d %s", value, m_app.tr("UNIT_DUR").c_str()); return text;
    default: return std::string();
    }
}

// FUN 00430420 texture set for a grade chassis takes mean of installed parts
const char* CarCraftScreen::gradeFolder(int grade) {
    if (grade < 5) return "_Basic";
    if (grade < 20) return "_Unique";
    if (grade < 65) return "_Epic";
    return "_Legend";
}

// kart instance and seven slots as one line a change rebuilds built car
std::string CarCraftScreen::previewToken() const {
    std::string token = std::to_string(m_config.kartInstance);
    for (uint32_t slot : m_config.slot) token += ' ' + std::to_string(slot);
    return token;
}

// FUN 004A5ED0 with factory flag Car FactoryCar CHASSIS model body then one nif per installed slot
bool CarCraftScreen::loadPreview() {
    m_parts.clear();
    m_previewCar = -1;
    m_sceneReady = false;
    // failed build waits for next config change instead of a retry every frame
    m_previewToken = previewToken();
    // stock car draws over pale box of Factory Car Back so that slice stands before stage camera
    const Texture* back = m_app.assets().texture("CarFactory/Factory_Car_Back.png");
    const Rect backRect = {kBackX, kBackY, back ? static_cast<float>(back->width) : 942.f,
                           back ? static_cast<float>(back->height) : 629.f};
    if (!loadPreviewSceneFor(m_app, m_view, m_previewWorld, kPreviewRect, kPreviewEye, kPreviewLook,
                             kPreviewField / kDegToRad, backArtPath(m_app), backRect,
                             previewHorizontalField() / kDegToRad))
        return false;
    Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    const std::string root = m_app.options().gameDir + "/Data/Public/Car/FactoryCar/";
    const OwnedKart* owned = cat.ownedKart(m_config.kartInstance);
    const KartRow* chassis = owned ? cat.kart(owned->kartKey) : nullptr;
    if (!chassis || chassis->model.empty()) { std::printf("[carcraft] the preset names no chassis kart\n"); return false; }
    // FUN 004A5ED0 with factory flag down builds plain kart body of Car Body High as char preview
    if (chassis->modelScheme == 0) {
        const DriverRow* driver = cat.driver(session.myDriverKey());
        const std::string asset = driver && !driver->asset.empty() ? driver->asset : std::string("Cosmo");
        m_previewCar = m_view.addCar(m_app.renderer(), m_app.options().gameDir, chassis->model, asset,
                                     charPreviewPaint(m_app, chassis->model));
        m_sceneReady = m_previewCar >= 0;
        std::printf("[carcraft] plain kart %s on the stage rect car %d\n", chassis->model.c_str(), m_previewCar);
        return m_sceneReady;
    }
    // chassis grade is mean of installed part grades as exe averages its seven lookups
    int sum = 0;
    int count = 0;
    for (uint32_t instance : m_config.slot) {
        const CarCraftPartInstance* part = instance ? cat.carCraftInstance(instance) : nullptr;
        if (!part) continue;
        sum += part->grade;
        ++count;
    }
    const int chassisGrade = count > 0 ? sum / count : 0;
    auto append = [&](const std::string& nif, const std::string& textureDir) {
        if (nif.empty()) return -1;
        KnC::Render::NifModelRequest request;
        request.nif_path = nif;
        request.texture_dir = textureDir;
        KnC::Render::PropModel model;
        std::string error;
        if (!KnC::Render::load_prop_model(request, model, error)) {
            std::printf("[carcraft] %s failed %s\n", nif.c_str(), error.c_str());
            return -1;
        }
        KnC::Tools::resolve_textures(textureDir, model);
        KnC::Tools::resolve_textures(std::filesystem::path(nif).parent_path().string(), model);
        return m_view.addProp(m_app.renderer(), model);
    };
    const std::string chassisDir = root + "CHASSIS/" + chassis->model;
    const std::string bodyNif = findEntryCi(chassisDir, "BODY.nif");
    const std::string chassisTex = root + "Texture/" + chassis->model + gradeFolder(chassisGrade);
    const int body = append(bodyNif, chassisTex);
    if (body < 0) return false;
    PreviewPart bodyPart;
    bodyPart.handle = body;
    m_parts.push_back(bodyPart);
    // six fixed slots ride chassis matrix tires ride O WHEEL dummies of body
    for (int category = 0; category < 7; ++category) {
        const int slot = slotOfCategory(category);
        const uint32_t instance = slot >= 0 ? m_config.slot[static_cast<size_t>(slot)] : 0;
        const CarCraftPartInstance* part = instance ? cat.carCraftInstance(instance) : nullptr;
        const CarCraftPartDef* def = part ? cat.carCraftPart(part->partKey) : nullptr;
        if (!def || def->model.empty()) continue;
        const CraftSlotArt& art = kSlotArt[category];
        const std::string dir = root + art.folder + "/" + def->model;
        const std::string tex = root + "Texture/" + def->model + gradeFolder(part->grade);
        for (int i = 0; i < art.count; ++i) {
            char file[64];
            if (art.count == 1) std::snprintf(file, sizeof(file), "%s.nif", art.file);
            else if (category == 2) std::snprintf(file, sizeof(file), "%s%d.nif", art.file, i + 1);
            else std::snprintf(file, sizeof(file), "%s%02d.nif", art.file, i + 1);
            const int handle = append(findEntryCi(dir, file), tex);
            if (handle < 0) continue;
            PreviewPart slotPart;
            slotPart.handle = handle;
            // car model wheel nodes read then car node set transform put each WHEEL on its O WHEEL dummy
            if (category == 2) {
                char dummy[16];
                std::snprintf(dummy, sizeof(dummy), "O_WHEEL%02d", i + 1);
                slotPart.hasLocal = KnC::Tools::ghost_car_dummy(bodyNif, dummy, slotPart.local);
            }
            m_parts.push_back(slotPart);
        }
    }
    m_sceneReady = true;
    std::printf("[carcraft] built %s grade %s with %zu parts\n", chassis->model.c_str(), gradeFolder(chassisGrade),
                m_parts.size() - 1);
    return true;
}

// the EngineDLL frustum takes the field times the aspect term as its horizontal angle
float CarCraftScreen::previewHorizontalField() const {
    return kPreviewField * (m_app.gameOptions().wideMode > 0.f ? kPreviewAspectWide : kPreviewAspect);
}

void CarCraftScreen::refreshPreview() {
    if (previewToken() != m_previewToken) loadPreview();
}

void CarCraftScreen::leave() {
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

void CarCraftScreen::sceneLost() {
    m_sceneReady = false;
    m_previewToken.clear();
}

// built car on stage rect arrows turn it wheels ride O WHEEL dummies
bool CarCraftScreen::drawScene() {
    refreshPreview();
    if (!m_sceneReady || (m_parts.empty() && m_previewCar < 0)) return false;
    // canvas rect in framebuffer pixels letterboxed or stretched as frame is
    float px[4];
    m_app.canvasToPixels(kPreviewRect.x, kPreviewRect.y, kPreviewRect.w, kPreviewRect.h, px);
    KnC::Render::ViewportRect box;
    box.x = static_cast<uint16_t>(px[0]);
    box.y = static_cast<uint16_t>(px[1]);
    box.width = static_cast<uint16_t>(px[2]);
    box.height = static_cast<uint16_t>(px[3]);
    const KnC::Render::ViewportRect& now = m_app.renderer().viewport();
    if (now.x != box.x || now.y != box.y || now.width != box.width || now.height != box.height)
        m_app.renderer().set_viewport(box);
    if (m_previewCar >= 0) {
        CarPose pose;
        pose.yawDeg = m_yaw;
        m_view.setPose(m_previewCar, pose, 0.f, 0.f);
    }
    float car[16];
    bx::mtxRotateZ(car, m_yaw * kDegToRad);
    for (const PreviewPart& part : m_parts) {
        if (part.handle < 0) continue;
        float world[16];
        if (part.hasLocal) bx::mtxMul(world, part.local, car);
        else for (int i = 0; i < 16; ++i) world[i] = car[i];
        m_view.placeProp(part.handle, world, true);
    }
    m_view.drawFixed(m_app.renderer(), kPreviewEye, kPreviewLook, kPreviewField, 1.f / 25.f, previewHorizontalField());
    return true;
}

void CarCraftScreen::update(float dt) {
    m_time += dt;
    // two arrows on stage turn built car while held
    const ButtonWidget* left = findAs<ButtonWidget>("btn_car_left");
    const ButtonWidget* right = findAs<ButtonWidget>("btn_car_right");
    if (left && left->state == ButtonWidget::State::Pressed) m_yaw -= kTurnSpeed * dt;
    if (right && right->state == ButtonWidget::State::Pressed) m_yaw += kTurnSpeed * dt;
    while (m_yaw < 0.f) m_yaw += 360.f;
    while (m_yaw >= 360.f) m_yaw -= 360.f;
    // scripted run takes its own shots auto capture would end run under script
    if (m_app.captureMode() && !m_app.scripted() && !m_captured && m_time > 1.2f) {
        m_captured = true;
        m_app.captureStage("carcraft");
        if (m_app.options().stopAt == "carcraft") m_app.finishRun();
    }
}

// back art carries factory car hole wallpaper shows through it as stock does
void CarCraftScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    AssetStore& assets = m_app.assets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    // back art keeps factory car hole open 3D lands under sprites as stock does
    const Rect hole = m_sceneReady ? kPreviewRect : Rect{0.f, 0.f, 0.f, 0.f};
    drawFrameBack(batch, assets, hole);
    for (Widget* w : m_order) if (w->visible && w->zIndex < 0 && w->zIndex > -19) w->draw(ctx);
    drawTextureWithHole(batch, assets.texture("CarFactory/Factory_Car_Back.png"), kBackX, kBackY, hole);
    sprite("CarFactory/Factory_Car_Top.png", kTopX, kTopY);
    const std::vector<CarCraftPreset>& presets = m_app.session().catalog().carCraftPresets();
    for (int i = 0; i < kSlotsShown; ++i) {
        const float x = kSlotX + kSlotStep * static_cast<float>(i);
        const size_t index = static_cast<size_t>(m_presetScroll + i);
        if (index >= presets.size()) { sprite("CarFactory/UI_FatoryCar_locked.png", x, kSlotY); continue; }
        // sub 42EFE0 one owned chassis per built slot wears the thumbnail an empty slot stays bare
        if (presets[index].slotState == 1) sprite("CarFactory/Factory_Car_Thumbnail.png", x, kSlotY);
        if (static_cast<int>(index) == m_preset) sprite("CarFactory/UI_select.png", x - 4.f, kSlotY - 4.f);
    }
    for (int i = 0; i < 8; ++i) {
        if (ButtonWidget* b = findAs<ButtonWidget>("btn_part" + std::to_string(i)))
            b->normal = assets.texture(std::string(kPartTabs[i].art) + (i == m_part ? "01.png" : "00.png"));
    }
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

void CarCraftScreen::drawOverlay(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    const std::vector<CarCraftPreset>& presets = m_app.session().catalog().carCraftPresets();
    auto sprite = [&](const char* path, float x, float y) {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
    };
    // sub 42EFE0 the name of a built slot centred on its blue bar empty until a rename
    for (int i = 0; i < kSlotsShown; ++i) {
        const size_t index = static_cast<size_t>(m_presetScroll + i);
        if (index >= presets.size() || presets[index].slotState != 1) continue;
        const float x = kSlotX + kSlotStep * static_cast<float>(i);
        drawAligned(ctx, ctx.bold, presets[index].name, x + kSlotNameDX - 2.f, kSlotNameY - 2.f, 16.f, kInkDark, Align::Centre);
        drawAligned(ctx, ctx.bold, presets[index].name, x + kSlotNameDX, kSlotNameY, 16.f, kInkWhite, Align::Centre);
    }
    const CarCraftPreset* row = preset();
    // sub 433140 a built slot prints its name on the plate an empty slot greys the plate out
    if (row && row->slotState == 1) {
        const std::string plate = m_renaming ? m_rename + "_" : row->name;
        drawAligned(ctx, ctx.bold, plate, kPlateNameX, kPlateNameY, 18.f, kInkWhite, Align::Centre);
    } else {
        sprite("CarFactory/Factory_Car_Machine_Dis.png", 399.f, 228.f);
    }
    // left list three rows the row art the icon the name and grade the period the count and the magnifier
    for (int i = 0; i < kListRows; ++i) {
        const size_t index = static_cast<size_t>(m_scroll + i);
        if (index >= m_rows.size()) break;
        const ListRow& r = m_rows[index];
        const float y = kListY + kListStep * static_cast<float>(i);
        sprite(static_cast<int>(index) == m_selected ? "CarFactory/Factory_Car_List_01.png" : "CarFactory/Factory_Car_List_00.png", kListX, y);
        const Texture* icon = r.icon.empty() ? nullptr : assets.texture(r.icon);
        if (icon && icon->valid()) ctx.batch.draw(icon->handle, kListX, y, 64.f, 60.f);
        // font slot 1 of 0x5CE5B8 Arial 15 weight 600 black on the white name box and the row
        ctx.bold.draw(ctx.batch, r.label, kRowTextX, y + 12.f, kRowFontPx, kInkBlack);
        if (m_part == 0) {
            const OwnedKart* owned = r.periodType == 3 ? m_app.session().catalog().ownedKart(r.instance) : nullptr;
            if (owned) drawDurability(ctx, assets, kRowDurX, y + 34.f, r.periodValue, kartDurabilityMax(m_app.session().catalog(), *owned));
        } else if (!r.active) {
            sprite("Garage/Garage_Itembox_Expiration.png", kListX, y);
            ctx.bold.draw(ctx.batch, m_app.tr("UNIT_EXPIRED"), kRowPeriodX, y + 38.f, kRowFontPx, kInkBlack);
        } else {
            ctx.bold.draw(ctx.batch, periodText(r.periodType, r.periodValue), kRowPeriodX, y + 38.f, kRowFontPx, kInkBlack);
        }
        if (m_part != 0 && r.count > 0)
            drawAligned(ctx, ctx.bold, "+" + std::to_string(r.count), kRowInfoX - 6.f, y + 37.f, 14.f, kInkWhite, Align::Right);
        if (r.inUse) sprite("CarFactory/Garage_Itembox_Use.png", kListX, y);
        sprite("CarFactory/Factory_Car_List_Info_00.png", kRowInfoX, y + 36.f);
    }
    if (m_rows.empty()) ctx.font.draw(ctx.batch, "nothing owned on this tab", kListX + 4.f, kListY + 24.f, 14.f, kInkWhite);
    // four bars for picked kart part bonuses are not on client
    auto bar = [&](const char* art, float x, float y, float fraction) {
        const Texture* t = assets.texture(art);
        if (t && t->valid()) ctx.batch.draw(t->handle, x, y, kBarW * fraction, static_cast<float>(t->height), kWhite, 0.f, 0.f, 1.f, 1.f);
        else ctx.batch.fill(x, y, kBarW * fraction, kBarH, rgba(220, 80, 80, 255));
    };
    std::array<float, 4> bars{65.f, 65.f, 65.f, 65.f};
    const Catalog& cat = m_app.session().catalog();
    if (const OwnedKart* kart = cat.ownedKart(m_config.kartInstance))
        if (const KartRow* def = cat.kart(kart->kartKey)) bars = cat.statBars(*def);
    bar("CarFactory/car_info_bar01_02.png", kBarLeftX, kBarTopY, bars[0] / 100.f);
    bar("CarFactory/car_info_bar02_02.png", kBarLeftX, kBarLowY, bars[1] / 100.f);
    bar("CarFactory/car_info_bar03_02.png", kBarRightX, kBarTopY, bars[2] / 100.f);
    bar("CarFactory/car_info_bar04_02.png", kBarRightX, kBarLowY, bars[3] / 100.f);
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 40.f, 706.f, 12.f, kInkGrey);
}

void CarCraftScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        for (int i = 0; i < kSlotsShown; ++i) {
            const Rect r = {kSlotX + kSlotStep * static_cast<float>(i), kSlotY, kSlotW, kSlotH};
            if (!r.contains(x, y)) continue;
            m_app.click();
            selectPreset(m_presetScroll + i);
            return;
        }
        for (int i = 0; i < kListRows; ++i) {
            const float rowY = kListY + kListStep * static_cast<float>(i);
            const size_t index = static_cast<size_t>(m_scroll + i);
            // sub 432B20 0x4326D1 the magnifier strictly inside 340 to 362 across 22 above the row foot
            if (x > kRowInfoX && x < kRowInfoX + 22.f && y > rowY + 36.f && y < rowY + 58.f && index < m_rows.size()) {
                m_app.click();
                m_selected = static_cast<int>(index);
                openPartInfo(m_rows[index]);
                return;
            }
            const Rect r = {kListX, rowY, 314.f, 60.f};
            if (!r.contains(x, y)) continue;
            if (index >= m_rows.size()) break;
            m_app.click();
            m_selected = static_cast<int>(index);
            return;
        }
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void CarCraftScreen::onChar(unsigned codepoint) {
    if (!m_renaming || codepoint < 32 || codepoint > 126) return;
    if (m_rename.size() >= 9) return;
    m_rename.push_back(static_cast<char>(codepoint));
}

void CarCraftScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS && m_renaming) {
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { submitRename(); return; }
        if (key == GLFW_KEY_ESCAPE) { m_renaming = false; return; }
        if (key == GLFW_KEY_BACKSPACE && !m_rename.empty()) { m_rename.pop_back(); return; }
        return;
    }
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) { exitToLobby(); return; }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { install(); return; }
        if (key == GLFW_KEY_BACKSPACE) { removePart(); return; }
        if (key == GLFW_KEY_UP && m_selected > 0) { --m_selected; if (m_selected < m_scroll) m_scroll = m_selected; return; }
        if (key == GLFW_KEY_DOWN && m_selected + 1 < static_cast<int>(m_rows.size())) {
            ++m_selected;
            if (m_selected >= m_scroll + kListRows) m_scroll = m_selected - kListRows + 1;
            return;
        }
    }
    WidgetScreen::onKey(key, action, mods);
}

void CarCraftScreen::onAction(const std::string& action, Widget& source) {
    if (action == "lobby" || action == "channel") { exitToLobby(); return; }
    if (action == "quit") { m_app.quit(); return; }
    if (action.rfind("part", 0) == 0) { m_part = action[4] - '0'; m_scroll = 0; m_selected = -1; refreshRows(); return; }
    if (action == "install") { install(); return; }
    if (action == "remove") { removePart(); return; }
    if (action == "save") { save(); return; }
    if (action == "machine") { beginRename(); return; }
    // small blue ball between two arrows turns built car back to its front
    if (action == "car_front") { m_yaw = 0.f; return; }
    if (action == "car_left" || action == "car_right") return;
    if (action == "list_up") { if (m_scroll > 0) --m_scroll; return; }
    if (action == "list_down") { if (m_scroll + kListRows < static_cast<int>(m_rows.size())) ++m_scroll; return; }
    if (action == "slot_left") { if (m_presetScroll > 0) --m_presetScroll; return; }
    if (action == "slot_right") {
        const int presets = static_cast<int>(m_app.session().catalog().carCraftPresets().size());
        if (m_presetScroll + kSlotsShown < presets) ++m_presetScroll;
        return;
    }
    if (frameAction(m_app, action)) return;
    m_status = source.id + " has no verb in this stage";
}

void CarCraftScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::BuyOk || event == SessionEvent::InventoryChanged) {
        refreshRows();
        return;
    }
    if (event == SessionEvent::CarCraftAck) {
        selectPreset(m_preset);
        m_status = "0x010A ack, " + std::to_string(m_app.session().catalog().carCraftPresets().size()) + " presets";
        return;
    }
    if (event == SessionEvent::CarCraftSaved) {
        selectPreset(m_preset);
        m_status = m_app.tr("MSG_SAVE_DONE");
    }
}

// stock leaves factory with lobby request 0x0012
void CarCraftScreen::exitToLobby() {
    m_app.session().openLobby();
    m_status = "0x0012 sent";
}

}
