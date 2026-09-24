#include "GarageScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "ui/MenuFrame.h"
#include "ui/MessagePopup.h"
#include "screens/ShopScreen.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace KnC::Client {

namespace {

// stock garage class sub 413340 top bar at 25 50 back at 37 88 on frame
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;
constexpr float kBackW = 942.f;
constexpr float kBackH = 629.f;
// owned grid six columns of 81 three rows of 79 from 450 193 cell art is 72 square
constexpr float kGridX = 450.f;
constexpr float kGridY = 193.f;
constexpr float kCellStepX = 81.f;
constexpr float kCellStepY = 79.f;
constexpr int kCols = 6;
constexpr int kRows = 3;
// detail block big icon 447 493 badge 542 493 name at 495 text at 620 521
constexpr float kBigX = 447.f;
constexpr float kBigY = 493.f;
constexpr float kNameX = 772.f;
constexpr float kNameY = 495.f;
constexpr float kDescX = 620.f;
constexpr float kDescY = 521.f;
constexpr float kDescW = 308.f;
constexpr float kGraphX = 444.f;
constexpr float kGraphY = 625.f;
// durability gauge for left column at 67 516
constexpr float kDurX = 67.f;
constexpr float kDurY = 516.f;
constexpr Rect kInstall = {679.f, 679.f, 115.f, 26.f};
// Buy slot from stage init at 793 679 same box shop opens in its mode one
constexpr Rect kBuy = {793.f, 679.f, 115.f, 26.f};
// detail box overlays ribbon at 447 494 In Use words 467 587 pairs 447 527
constexpr float kRibbonX = 447.f;
constexpr float kRibbonY = 494.f;
constexpr float kInUseX = 467.f;
constexpr float kInUseY = 587.f;
constexpr float kPairsX = 447.f;
constexpr float kPairsY = 527.f;
constexpr Rect kPageLeft = {628.f, 439.f, 24.f, 24.f};
constexpr Rect kPageRight = {729.f, 439.f, 24.f, 24.f};
// stock durability field runs from zero to five hundred
constexpr int kDurabilityMax = 500;
// auto walk waits this long between two staged steps
constexpr float kStepSeconds = 0.8f;

// four JSON category buttons in order char car item and rest
const char* const kCategoryButtons[4] = {"button_12", "button_13", "button_14", "button_15"};
// character sub tabs char head cloth face back pet
const char* const kCharSubButtons[6] = {"button_16", "button_17", "button_18", "button_19", "button_20", "button_21"};
// kart sub tabs kart antenna number paint
const char* const kKartSubButtons[4] = {"button_22", "button_23", "button_24", "button_25"};

}

// Common Car Info Back with one pixel bar columns 132 wide at four stock spots
void drawKartGraph(DrawContext& ctx, AssetStore& assets, float x, float y, const float* bars) {
    const Texture* back = assets.texture("CarGraph/Common_Car_Info_Back.png");
    if (back && back->valid()) ctx.batch.draw(back->handle, x, y, static_cast<float>(back->width), static_cast<float>(back->height));
    // FUN 00429040 draws speed top left handling bottom left drift top right booster bottom right
    const float bx[4] = {x + 76.f, x + 76.f, x + 346.f, x + 346.f};
    const float by[4] = {y + 6.f, y + 28.f, y + 6.f, y + 28.f};
    for (int i = 0; i < 4; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "CarGraph/Common_Car_Info_Bar_%02d.png", i + 1);
        const Texture* bar = assets.texture(name);
        const float w = std::min(131.f, std::floor(bars[i]));
        if (w <= 0.f) continue;
        if (bar && bar->valid()) ctx.batch.draw(bar->handle, bx[i], by[i], w, static_cast<float>(bar->height));
        else ctx.batch.fill(bx[i], by[i], w, 16.f, rgba(220, 80, 80, 255));
    }
}

// stock icon bank from 0x42AFE0 one reg icon per ability id gaps stay empty
const char* const kAbilityIcons[26] = {
    "Icon/reg_ice_01.png", "Icon/reg_smog.png", "Icon/reg_shield_01.png", "Icon/reg_rabbit_01.png", "Icon/reg_turtle_01.png",
    "Icon/reg_storm_01.png", "Icon/reg_rocket_01.png", "Icon/reg_thunder_01.png", "Icon/reg_exp.png", "Icon/reg_peri.png",
    "Icon/reg_exp.png", "Icon/reg_peri.png", "Icon/reg_turtle_booster.png", "Icon/reg_rocket_turtle.png", "Icon/reg_rocket_rocket.png",
    "Icon/bonus_exp.png", "Icon/bonus_peri.png", "Icon/bonus_exp_peri.png", "Icon/bonus_exp_peri.png", "", "", "", "",
    "Icon/reg_hammer_01.png", "Icon/reg_turtle_rabbit.png", "Icon/reg_ice_turtle.png"};

void drawAbilityPairs(DrawContext& ctx, AssetStore& assets, float x, float y, const int* ids, const int* percents) {
    for (int i = 0; i < 2; ++i) {
        const int id = ids[i];
        if (id < 0 || id > 25 || kAbilityIcons[id][0] == 0) continue;
        const float px = x + 76.f * static_cast<float>(i);
        const Texture* icon = assets.texture(kAbilityIcons[id]);
        if (icon && icon->valid()) ctx.batch.draw(icon->handle, px, y, static_cast<float>(icon->width), static_cast<float>(icon->height));
        const std::string text = std::to_string(percents[i]) + "%";
        const float w = ctx.font.measure(text, 13.f) + 3.f;
        ctx.batch.fill(px + 40.f, y + 27.f, w, 13.f, rgba(0, 0, 0, 160));
        ctx.font.draw(ctx.batch, text, px + 42.f, y + 26.f, 13.f, kInkWhite);
    }
}

// 0x42AD20 Endurance Back then one column per pixel from plus 29 5 green from 70% amber from 30% else red
void drawDurability(DrawContext& ctx, AssetStore& assets, float x, float y, int durability, int maximum) {
    const Texture* back = assets.texture("DurGraph/Common_Endurance_Back.png");
    if (back && back->valid()) ctx.batch.draw(back->handle, x, y, static_cast<float>(back->width), static_cast<float>(back->height));
    const float ratio = maximum > 0 ? std::min(1.f, std::max(0.f, static_cast<float>(durability) / static_cast<float>(maximum))) : 0.f;
    const char* art = ratio >= 0.7f ? "DurGraph/Common_Endurance_Bar_01.png"
                                    : (ratio >= 0.3f ? "DurGraph/Common_Endurance_Bar_02.png" : "DurGraph/Common_Endurance_Bar_03.png");
    const Texture* bar = assets.texture(art);
    const float w = std::floor(ratio * 93.f);
    if (w <= 0.f) return;
    if (bar && bar->valid()) ctx.batch.draw(bar->handle, x + 29.f, y + 5.f, w, static_cast<float>(bar->height));
    else ctx.batch.fill(x + 29.f, y + 5.f, w, 16.f, rgba(80, 220, 80, 255));
}

int kartDurabilityMax(const Catalog& cat, const OwnedKart& kart) {
    const KartRow* def = cat.kart(kart.kartKey);
    if (def && !def->prices.empty() && def->prices[0].periodValue > 0) return static_cast<int>(def->prices[0].periodValue);
    return kDurabilityMax;
}

void GarageScreen::enter() {
    loadLayout("ui_state_06_garage.json");
    AssetStore& assets = m_app.assets();
    // JSON lists nine sprites at origin screen draws those itself
    for (int i = 0; i <= 5; ++i) if (Widget* w = find("image_" + std::to_string(i))) w->visible = false;
    for (int i = 27; i <= 29; ++i) if (Widget* w = find("image_" + std::to_string(i))) w->visible = false;
    for (int i = 0; i < 4; ++i) if (Widget* w = find(kCategoryButtons[i])) w->action = "cat" + std::to_string(i);
    for (int i = 0; i < 6; ++i) if (Widget* w = find(kCharSubButtons[i])) w->action = "sub" + std::to_string(i);
    for (int i = 0; i < 4; ++i) if (Widget* w = find(kKartSubButtons[i])) w->action = "sub" + std::to_string(i);
    if (Widget* w = find("button_26")) w->action = "sub0";
    addMenuFrame(*this, assets, FrameMode::Full, "garage");

    auto button = [&](const char* id, const char* action, const char* art, const Rect& rect) {
        auto b = std::make_unique<ButtonWidget>();
        b->type = ElementType::Button;
        b->id = id;
        b->action = action;
        b->normal = assets.texture(std::string(art) + "00.png");
        b->hover = assets.texture(std::string(art) + "01.png");
        b->pressed = assets.texture(std::string(art) + "02.png");
        b->rect = rect;
        b->zIndex = 30;
        add(std::move(b));
    };
    button("btn_install", "install", "Garage/Garage_Install_", kInstall);
    button("btn_remove", "remove", "Garage/Garage_Remove_", kInstall);
    button("btn_delete", "delete", "Garage/Garage_Delete_", kInstall);
    button("btn_buy", "buy", "Garage/Garage_Buy_", kBuy);
    addCharPanelWidgets(*this, assets);
    m_users = UserListState();
    hookUserListTap(m_app, &m_users);
    m_orbit = 0.f;
    button("btn_page_left", "page_left", "Buttons/Common_Page_Left_", kPageLeft);
    button("btn_page_right", "page_right", "Buttons/Common_Page_Right_", kPageRight);

    m_time = 0.f;
    m_step = 0;
    m_page = 0;
    m_status.clear();
    m_lookToken = charPreviewLookToken(m_app);
    m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
    selectTab(1, 0);
    refreshPreview();
}

void GarageScreen::leave() {
    m_app.setFrameTap(nullptr);
    // race and every other screen want whole window back
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

uint32_t GarageScreen::tabCategory() const {
    if (m_category == 0) return m_sub == 0 ? 0u : (m_sub == 5 ? 4u : 3u);
    if (m_category == 1) return m_sub == 0 ? 1u : 3u;
    if (m_category == 2) return 2u;
    return 3u;
}

const OwnedKart* GarageScreen::selectedKart() const {
    const Session& session = m_app.session();
    const int32_t instance = session.profile().kartInstance;
    if (instance >= 0) {
        if (const OwnedKart* k = session.catalog().ownedKart(static_cast<uint32_t>(instance))) return k;
    }
    return session.catalog().ownedKarts().empty() ? nullptr : &session.catalog().ownedKarts().front();
}

const OwnedCharacter* GarageScreen::selectedCharacter() const {
    const Session& session = m_app.session();
    const int32_t instance = session.profile().characterInstance;
    if (instance >= 0) {
        if (const OwnedCharacter* c = session.catalog().ownedCharacter(static_cast<uint32_t>(instance))) return c;
    }
    return session.catalog().ownedCharacters().empty() ? nullptr : &session.catalog().ownedCharacters().front();
}

void GarageScreen::selectTab(int category, int sub) {
    m_category = category;
    m_sub = sub;
    m_page = 0;
    m_selected = -1;
    refreshCells();
    refreshButtons();
}

void GarageScreen::refreshButtons() {
    const bool charTab = m_category == 0;
    const bool kartTab = m_category == 1;
    for (int i = 0; i < 4; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kCategoryButtons[i])) b->active = i == m_category;
    for (int i = 0; i < 6; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kCharSubButtons[i])) b->active = charTab && i == m_sub;
    for (int i = 0; i < 4; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kKartSubButtons[i])) b->active = kartTab && i == m_sub;
    if (ButtonWidget* b = findAs<ButtonWidget>("button_26")) b->active = m_category == 2;
    for (int i = 0; i < 6; ++i) if (Widget* w = find(kCharSubButtons[i])) w->visible = charTab;
    for (int i = 0; i < 4; ++i) if (Widget* w = find(kKartSubButtons[i])) w->visible = kartTab;
    if (Widget* w = find("button_26")) w->visible = m_category == 2;
    const bool picked = m_selected >= 0 && m_selected < static_cast<int>(m_cells.size());
    const bool equipped = picked && m_cells[static_cast<size_t>(m_selected)].equipped;
    const bool dead = picked && !m_cells[static_cast<size_t>(m_selected)].active;
    // sub 413D30 dead row shows Delete live one Install or Remove worn driver or kart shows nothing
    if (Widget* w = find("btn_delete")) w->visible = dead && !m_deleted.count(m_cells[static_cast<size_t>(m_selected)].baseKey);
    if (Widget* w = find("btn_install")) w->visible = picked && !dead && !equipped;
    if (Widget* w = find("btn_remove")) w->visible = picked && !dead && equipped && tabCategory() >= 2;
    if (Widget* w = find("btn_buy")) w->visible = picked && canExtend(m_cells[static_cast<size_t>(m_selected < 0 ? 0 : m_selected)]);
}

void GarageScreen::refreshCells() {
    m_cells.clear();
    const Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    const OwnedCharacter* mine = selectedCharacter();
    const OwnedKart* kart = selectedKart();

    auto pushParts = [&](int category, int sub) {
        for (const OwnedPart& owned : cat.ownedParts()) {
            const PartRow* def = cat.part(owned.partKey);
            if (!def) continue;
            if (sub >= 0 && !partInTab(*def, category, sub)) continue;
            Cell c;
            c.baseKey = def->key;
            c.instance = owned.instance;
            c.label = m_app.tr(def->nameKey);
            c.icon = partIcon(*def);
            c.bigIcon = partIcon(*def, true);
            c.descKey = def->descKey;
            c.requiredLevel = def->requiredLevel;
            c.active = owned.active != 0;
            if (mine) for (uint32_t worn : mine->accessory) if (worn == def->key) c.equipped = true;
            if (kart) {
                for (uint32_t worn : kart->part) if (worn == def->key) c.equipped = true;
                for (uint32_t worn : kart->item) if (worn == def->key) c.equipped = true;
            }
            if (owned.periodMode == 2) c.note = std::to_string(owned.periodValue) + " uses";
            m_cells.push_back(c);
        }
    };

    if (m_category == 0 && m_sub == 0) {
        for (const OwnedCharacter& owned : cat.ownedCharacters()) {
            const DriverRow* def = cat.driver(owned.driverKey);
            Cell c;
            c.baseKey = owned.driverKey;
            c.instance = owned.instance;
            c.label = def ? m_app.tr(def->nameKey) : "driver " + std::to_string(owned.driverKey);
            c.icon = def ? driverIcon(*def) : std::string();
            c.bigIcon = def ? driverIcon(*def, true) : std::string();
            c.descKey = def ? def->descKey : std::string();
            c.requiredLevel = def ? def->requiredLevel : 0;
            c.equipped = mine && mine->instance == owned.instance;
            c.active = owned.active != 0;
            m_cells.push_back(c);
        }
    } else if (m_category == 0 && m_sub == 5) {
        // 0x0104 owned pets equipped flag of a row tested against one
        for (const OwnedPet& owned : cat.ownedPets()) {
            const PetRow* def = cat.pet(owned.petKey);
            Cell c;
            c.baseKey = owned.petKey;
            c.instance = owned.instance;
            c.label = def ? m_app.tr(def->nameKey) : "pet " + std::to_string(owned.petKey);
            c.icon = def ? petIcon(*def) : std::string();
            c.bigIcon = def ? petIcon(*def, true) : std::string();
            c.descKey = def ? def->descKey : std::string();
            c.equipped = owned.equipped == 1;
            c.active = owned.active != 0;
            if (owned.periodMode == 1) c.note = std::to_string(owned.periodValue) + " days";
            m_cells.push_back(c);
        }
    } else if (m_category == 0) {
        pushParts(0, m_sub);
    } else if (m_category == 1 && m_sub == 0) {
        // sub 416BB0 the built factory slots first then the owned karts that are no chassis
        const std::vector<CarCraftPreset>& presets = cat.carCraftPresets();
        for (size_t i = 0; i < presets.size(); ++i) {
            const OwnedKart* owned = presets[i].slotState != 0 ? cat.ownedKart(presets[i].kartInstance) : nullptr;
            const KartRow* def = owned ? cat.kart(owned->kartKey) : nullptr;
            if (!def) continue;
            Cell c;
            c.baseKey = owned->kartKey;
            c.instance = owned->instance;
            c.label = presets[i].name;
            c.icon = kartIcon(*def);
            c.bigIcon = kartIcon(*def, true);
            c.requiredLevel = def->requiredLevel;
            c.equipped = kart && kart->instance == owned->instance;
            c.active = owned->active != 0;
            c.preset = static_cast<int>(i);
            m_cells.push_back(c);
        }
        for (const OwnedKart& owned : cat.ownedKarts()) {
            const KartRow* def = cat.kart(owned.kartKey);
            if (def && def->modelScheme == 1) continue;
            Cell c;
            c.baseKey = owned.kartKey;
            c.instance = owned.instance;
            c.label = def ? m_app.tr(def->nameKey) : "kart " + std::to_string(owned.kartKey);
            c.icon = def ? kartIcon(*def) : std::string();
            c.bigIcon = def ? kartIcon(*def, true) : std::string();
            c.descKey = def ? def->descKey : std::string();
            c.requiredLevel = def ? def->requiredLevel : 0;
            c.equipped = kart && kart->instance == owned.instance;
            c.active = owned.active != 0;
            if (owned.expiryKind == 3) c.note = std::to_string(owned.durability) + " dur";
            m_cells.push_back(c);
        }
    } else if (m_category == 1) {
        pushParts(1, m_sub);
    } else if (m_category == 2) {
        for (const OwnedItem& owned : cat.ownedItems()) {
            const ItemRow* def = cat.item(owned.itemKey);
            Cell c;
            c.baseKey = owned.itemKey;
            c.instance = owned.instance;
            c.label = def ? m_app.tr(def->nameKey) : "item " + std::to_string(owned.itemKey);
            c.icon = def ? pickIcon(m_app.assets(), itemIcon(*def), "Parts/item_" + def->nameKey + "_01.png") : std::string();
            c.bigIcon = def ? pickIcon(m_app.assets(), itemIcon(*def, true), c.icon) : std::string();
            c.descKey = def ? def->descKey : std::string();
            c.requiredLevel = def ? def->requiredLevel : 0;
            c.equipped = owned.inUse != 0;
            c.active = owned.active != 0;
            if (owned.periodMode == 2) c.note = std::to_string(owned.periodValue) + " left";
            m_cells.push_back(c);
        }
    } else {
        pushParts(0, -1);
    }
    if (m_selected >= static_cast<int>(m_cells.size())) m_selected = -1;
    if (m_selected < 0 && !m_cells.empty()) {
        for (size_t i = 0; i < m_cells.size(); ++i) if (m_cells[i].equipped) m_selected = static_cast<int>(i);
        if (m_selected < 0) m_selected = 0;
    }
}

void GarageScreen::refreshPreview() {
    if (!m_sceneReady) return;
    // equip changes worn parts so driver model is built again with them
    const std::string token = charPreviewLookToken(m_app);
    if (token != m_lookToken) {
        m_lookToken = token;
        m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
        m_previewCar = -1;
        m_previewKart.clear();
        m_previewDriver.clear();
        if (!m_sceneReady) return;
    }
    const Session& session = m_app.session();
    const OwnedKart* kart = selectedKart();
    const OwnedCharacter* mine = selectedCharacter();
    uint32_t kartInstance = kart ? kart->instance : 0;
    // sub 413120 picked kart tile swaps preview kart other tabs keep worn one
    if (m_category == 1 && m_sub == 0 && m_selected >= 0 && m_selected < static_cast<int>(m_cells.size()))
        if (session.catalog().ownedKart(m_cells[static_cast<size_t>(m_selected)].instance))
            kartInstance = m_cells[static_cast<size_t>(m_selected)].instance;
    const DriverRow* driverDef = mine ? session.catalog().driver(mine->driverKey) : nullptr;
    // a factory kart builds from its preset row so the chassis and the parts show
    const std::string model = ownedKartViewModel(m_app, kartInstance);
    const std::string asset = driverDef && !driverDef->asset.empty() ? driverDef->asset : std::string("Cosmo");
    if (model == m_previewKart && asset == m_previewDriver && m_previewCar >= 0) return;
    if (m_previewCar >= 0) m_view.removeCar(m_previewCar);
    m_previewKart = model;
    m_previewDriver = asset;
    // paint goes in with car so first drawn frame swaps no body and uploads nothing
    m_previewCar = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset, charPreviewPaint(m_app, model));
    CarPose pose;
    m_view.setPose(m_previewCar, pose, 0.f, 0.f);
    std::printf("[garage] preview kart %s driver %s\n", model.c_str(), asset.c_str());
}

void GarageScreen::update(float dt) {
    m_time += dt;
    m_orbit = turnCharPreview(*this, m_orbit, dt);
    if (!m_app.captureMode() || m_app.scripted()) return;
    // one staged step per step gap tab change and its picture never share a frame
    if (m_time < static_cast<float>(m_step) * kStepSeconds + 1.2f) return;
    switch (m_step) {
    case 0: m_app.captureStage("garage"); break;
    case 1: selectTab(0, 0); break;
    case 2: m_app.captureStage("garage2"); break;
    case 3: selectTab(2, 0); break;
    case 4: m_app.captureStage("garage3"); break;
    case 5: selectTab(3, 0); break;
    case 6: m_app.captureStage("garage4"); break;
    case 7:
        if (const OwnedCharacter* mine = selectedCharacter()) m_app.session().equipUse(0, mine->driverKey);
        m_status = "auto select sent 0x00B9 for the driver";
        break;
    case 8:
        if (const OwnedKart* kart = selectedKart()) m_app.session().equipUse(1, kart->kartKey);
        m_status = "auto select sent 0x00B9 for the kart";
        break;
    case 9: selectTab(1, 0); break;
    case 10:
        m_app.captureStage("garage5");
        if (m_app.options().stopAt == "garage") m_app.finishRun();
        break;
    default: break;
    }
    ++m_step;
}

// popup drew its own scene over stage so preview scene and kart go up again
void GarageScreen::sceneLost() {
    m_lookToken = charPreviewLookToken(m_app);
    m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
    m_previewCar = -1;
    m_previewKart.clear();
    m_previewDriver.clear();
    refreshPreview();
}

bool GarageScreen::drawScene() {
    if (!m_sceneReady || m_previewCar < 0 || m_users.open) return false;
    drawCharPreview(m_app, m_view, m_previewCar, m_orbit);
    return true;
}

// back art keeps hole of preview frame top bar goes over it frame under it
void GarageScreen::drawBack(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    const Rect hole = m_users.open ? Rect{0.f, 0.f, 0.f, 0.f} : kCharPanelHole;
    drawTextureWithHole(batch, assets.texture("Garage/Garage_Back.png"), kBackX, kBackY, hole);
    const Texture* top = assets.texture("Garage/Garage_Top.png");
    if (top && top->valid()) batch.draw(top->handle, kTopX, kTopY, static_cast<float>(top->width), static_cast<float>(top->height));
}

void GarageScreen::draw(SpriteBatch& batch) {
    // frame widgets sit at negative z back must land between them and tabs
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    // wallpaper and common back keep preview hole open 3D lands under sprites
    drawFrameBack(batch, m_app.assets(), m_users.open ? Rect{0.f, 0.f, 0.f, 0.f} : kCharPanelHole);
    for (Widget* w : m_order) if (w->visible && w->zIndex < 0 && w->zIndex > -19) w->draw(ctx);
    drawBack(batch);
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

Rect GarageScreen::cellRect(size_t index) const {
    const int column = static_cast<int>(index) % kCols;
    const int row = static_cast<int>(index) / kCols;
    return {kGridX + static_cast<float>(column) * kCellStepX, kGridY + static_cast<float>(row) * kCellStepY, 72.f, 72.f};
}

void GarageScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        const size_t first = static_cast<size_t>(m_page * kCols * kRows);
        for (size_t i = 0; i < static_cast<size_t>(kCols * kRows); ++i) {
            const size_t index = first + i;
            if (index >= m_cells.size()) break;
            if (!cellRect(i).contains(x, y)) continue;
            m_selected = static_cast<int>(index);
            refreshButtons();
            refreshPreview();
            return;
        }
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void GarageScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) { exitGarage(); return; }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { install(); return; }
        if (key == GLFW_KEY_BACKSPACE) { remove(); return; }
        if (key == GLFW_KEY_TAB) { selectTab((m_category + 1) % 4, 0); return; }
        if (key == GLFW_KEY_LEFT && m_selected > 0) { --m_selected; refreshButtons(); refreshPreview(); return; }
        if (key == GLFW_KEY_RIGHT && m_selected + 1 < static_cast<int>(m_cells.size())) { ++m_selected; refreshButtons(); refreshPreview(); return; }
    }
    WidgetScreen::onKey(key, action, mods);
}

void GarageScreen::onAction(const std::string& passedAction, Widget& source) {
    // loadLayout caught button action when JSON had none so read live one enter set on tab
    const std::string& action = source.action.empty() ? passedAction : source.action;
    if (action.rfind("cat", 0) == 0) { selectTab(action[3] - '0', 0); return; }
    if (action.rfind("sub", 0) == 0) { selectTab(m_category, action[3] - '0'); return; }
    if (action == "install") { install(); return; }
    if (action == "remove") { remove(); return; }
    if (action == "delete") { deleteRow(); return; }
    if (action == "buy") { buyRow(); return; }
    if (charPanelAction(*this, m_app, m_users, action, &m_orbit)) return;
    if (action == "lobby" || action == "quit") { exitGarage(); return; }
    if (action == "shop") { m_app.session().openShop(); m_status = "shop 0x0010 sent"; return; }
    if (action == "page_left") { if (m_page > 0) --m_page; return; }
    if (action == "page_right") {
        const int pages = (static_cast<int>(m_cells.size()) + kCols * kRows - 1) / (kCols * kRows);
        if (m_page + 1 < pages) ++m_page;
        return;
    }
    if (frameAction(m_app, action)) return;
    m_status = source.id + " is not part of this phase";
}

void GarageScreen::install() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_cells.size())) { m_status = "pick a row first"; return; }
    const uint32_t category = tabCategory();
    const Cell& cell = m_cells[static_cast<size_t>(m_selected)];
    // sub 412BC0 0x00C1 use type four to seven is repair scroll asks first
    if (category == 2 && !m_confirmRepair) {
        const ItemRow* def = m_app.session().catalog().item(cell.baseKey);
        if (def && def->useType >= 4 && def->useType <= 7) {
            const OwnedKart* kart = selectedKart();
            if (!kart || kart->expiryKind != 3) { m_status = "the selected kart has no durability bar"; return; }
            m_confirmRepair = true;
            m_app.pushScreen(std::make_unique<MessagePopup>(m_app, m_app.tr("MSG_REPAIR_USE"), [this]() { install(); },
                                                            [this]() { m_confirmRepair = false; }, true));
            return;
        }
    }
    m_confirmRepair = false;
    m_app.session().equipUse(category, cell.baseKey);
    m_status = "0x00B9 category " + std::to_string(category) + " key " + std::to_string(cell.baseKey);
}

void GarageScreen::remove() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_cells.size())) { m_status = "pick a row first"; return; }
    const uint32_t category = tabCategory();
    if (category == 0 || category == 1) { m_status = "the character and the kart cannot be taken off"; return; }
    const Cell& cell = m_cells[static_cast<size_t>(m_selected)];
    m_app.session().unequip(category, cell.baseKey);
    m_status = "0x00BA category " + std::to_string(category) + " key " + std::to_string(cell.baseKey);
}

// sub 412E00 sends 0x00B8 category and base key for a dead row only the ack removes the row
void GarageScreen::deleteRow() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_cells.size())) { m_status = "pick a row first"; return; }
    const Cell& cell = m_cells[static_cast<size_t>(m_selected)];
    if (cell.active) { m_status = "only a dead row can be deleted"; return; }
    const uint32_t category = tabCategory();
    m_app.session().deleteOwned(category, cell.baseKey);
    // the Delete button hides until the ack lands so a double click sends one verb
    m_deleted.insert(cell.baseKey);
    m_status = "0x00B8 category " + std::to_string(category) + " key " + std::to_string(cell.baseKey);
    refreshButtons();
}

// sub 414630 the Buy slot the def is still sold the row is not permanent and the level passes
bool GarageScreen::canExtend(const Cell& cell) const {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_cells.size())) return false;
    const Session& session = m_app.session();
    const Catalog& cat = session.catalog();
    if (cell.requiredLevel > session.profile().level) return false;
    const uint32_t category = tabCategory();
    switch (category) {
    case 0: {
        const OwnedCharacter* owned = cat.ownedCharacter(cell.instance);
        return cat.driver(cell.baseKey) != nullptr && owned && owned->limitType != 0;
    }
    case 1: {
        const OwnedKart* owned = cat.ownedKart(cell.instance);
        // a durability kart is bought again through its own price row not through this slot
        return cat.kart(cell.baseKey) != nullptr && owned && owned->expiryKind != 0 && owned->expiryKind != 3;
    }
    case 2: {
        for (const OwnedItem& owned : cat.ownedItems())
            if (owned.instance == cell.instance) return cat.item(cell.baseKey) != nullptr && owned.periodMode != 0;
        return false;
    }
    case 4: {
        const OwnedPet* owned = cat.ownedPet(cell.instance);
        return cat.pet(cell.baseKey) != nullptr && owned && owned->periodMode != 0;
    }
    default: {
        for (const OwnedPart& owned : cat.ownedParts())
            if (owned.instance == cell.instance) return cat.part(cell.baseKey) != nullptr && owned.periodMode != 0;
        return false;
    }
    }
}

// FUN 00412ED0 opens the shared item box in mode one its OK sends 0x00B7 with the picked price row
void GarageScreen::buyRow() {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_cells.size())) { m_status = "pick a row first"; return; }
    const Cell& cell = m_cells[static_cast<size_t>(m_selected)];
    const Catalog& cat = m_app.session().catalog();
    ShopTile tile;
    tile.category = tabCategory();
    tile.baseKey = cell.baseKey;
    tile.requiredLevel = cell.requiredLevel;
    tile.label = cell.label;
    tile.icon = cell.icon;
    tile.bigIcon = cell.bigIcon;
    tile.descKey = cell.descKey;
    tile.owned = true;
    switch (tile.category) {
    case 0: if (const DriverRow* r = cat.driver(cell.baseKey)) tile.quotes = quotesOf(cat, r->prices); break;
    case 1: if (const KartRow* r = cat.kart(cell.baseKey)) tile.quotes = quotesOf(cat, r->prices); break;
    case 2: if (const ItemRow* r = cat.item(cell.baseKey)) tile.quotes = quotesOf(cat, r->prices); break;
    case 4: if (const PetRow* r = cat.pet(cell.baseKey)) tile.quotes = quotesOf(cat, r->prices); break;
    default: if (const PartRow* r = cat.part(cell.baseKey)) tile.quotes = quotesOf(cat, r->prices); break;
    }
    if (tile.quotes.empty()) { m_status = "that row carries no price option"; return; }
    m_app.pushScreen(std::make_unique<ShopItemPopup>(m_app, tile, true));
    m_status = "the price options of " + cell.label + " are open";
}

void GarageScreen::exitGarage() {
    m_status = "leaving 0x0012";
    m_app.session().openLobby();
}

void GarageScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::InventoryChanged || event == SessionEvent::BuyOk) {
        m_deleted.clear();
        refreshCells();
        refreshButtons();
        refreshPreview();
    }
}

void GarageScreen::drawOverlay(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    const Session& session = m_app.session();
    const OwnedKart* kart = selectedKart();
    drawCharPanel(ctx, assets, m_users, &m_app.session());
    // the stock capture shows no gauge on a permanent kart only a durability priced one wears it
    if (!m_users.open && kart && kart->expiryKind == 3)
        drawDurability(ctx, assets, kDurX, kDurY, kart->durability, kartDurabilityMax(session.catalog(), *kart));
    drawCharInfo(ctx, assets, session.profile());
    drawGrid(ctx);
    drawDetail(ctx);
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 450.f, 466.f, 12.f, kInkGrey);
}

void GarageScreen::drawGrid(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    const Texture* box = assets.texture("Garage/Garage_Itembox_00.png");
    const Texture* boxOn = assets.texture("Garage/Garage_Itembox_01.png");
    const Texture* used = assets.texture("Garage/Garage_Itembox_Use.png");
    const size_t perPage = static_cast<size_t>(kCols * kRows);
    const size_t first = static_cast<size_t>(m_page) * perPage;
    for (size_t i = 0; i < perPage; ++i) {
        const size_t index = first + i;
        if (index >= m_cells.size()) break;
        const Cell& cell = m_cells[index];
        const Rect r = cellRect(i);
        const bool picked = static_cast<int>(index) == m_selected;
        const Texture* art = picked && boxOn && boxOn->valid() ? boxOn : box;
        if (art && art->valid()) ctx.batch.draw(art->handle, r.x, r.y, r.w, r.h);
        const Texture* icon = cell.icon.empty() ? nullptr : assets.texture(cell.icon);
        if (icon && icon->valid()) ctx.batch.draw(icon->handle, r.x + 4.f, r.y + 4.f, 64.f, 64.f);
        // the stock tile wears the small level flag top right and the in use ball top left
        char flag[48];
        std::snprintf(flag, sizeof(flag), "Icon/lv_icon_s_%03d.png", std::min(std::max(static_cast<int>(cell.requiredLevel), 1), 50));
        const Texture* level = assets.texture(flag);
        if (level && level->valid() && cell.requiredLevel > 0) ctx.batch.draw(level->handle, r.x + 46.f, r.y + 1.f, static_cast<float>(level->width), static_cast<float>(level->height));
        if (cell.equipped && used && used->valid()) ctx.batch.draw(used->handle, r.x + 3.f, r.y + 5.f, 22.f, 22.f);
    }
    const int pages = std::max(1, (static_cast<int>(m_cells.size()) + static_cast<int>(perPage) - 1) / static_cast<int>(perPage));
    drawAligned(ctx, ctx.bold, std::to_string(m_page + 1) + " / " + std::to_string(pages), 691.f, 442.f, 16.f, kInkDark, Align::Centre);
    if (m_cells.empty()) ctx.font.draw(ctx.batch, "nothing owned in this tab", kGridX + 12.f, kGridY + 30.f, 14.f, kInkGrey);
}

void GarageScreen::drawDetail(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    if (m_selected < 0 || m_selected >= static_cast<int>(m_cells.size())) return;
    const Cell& cell = m_cells[static_cast<size_t>(m_selected)];
    const Texture* icon = cell.bigIcon.empty() ? nullptr : assets.texture(cell.bigIcon);
    if (!icon) icon = cell.icon.empty() ? nullptr : assets.texture(cell.icon);
    if (icon && icon->valid()) ctx.batch.draw(icon->handle, kBigX, kBigY, 128.f, 128.f);
    const KartRow* kartDef = m_app.session().catalog().kart(cell.baseKey);
    const bool kartRow = kartDef && m_category == 1 && m_sub == 0;
    // a kart row wears the period ribbon the two ability pairs and the In Use words
    if (kartRow) {
        const OwnedKart* owned = m_app.session().catalog().ownedKart(cell.instance);
        // garage item detail draw 0x415FF0 a mode 3 kart shows the wrench bar where the period ribbon goes
        if (owned && owned->expiryKind == 3) {
            drawDurability(ctx, assets, kRibbonX + 1.f, kRibbonY, owned->durability,
                           kartDurabilityMax(m_app.session().catalog(), *owned));
        } else {
            const char* ribbon = owned && owned->expiryKind == 1 ? "Garage/days.png" : owned && owned->expiryKind == 2 ? "Garage/times.png" : "Garage/permanent.png";
            const Texture* period = assets.texture(ribbon);
            if (period && period->valid()) ctx.batch.draw(period->handle, kRibbonX, kRibbonY, static_cast<float>(period->width), static_cast<float>(period->height));
        }
        // the two ability pairs of the 0x00C0 row of a kart cell an id off 0 to 25 hides
        const KartRow* kart = m_category == 1 && m_sub == 0 ? m_app.session().catalog().kart(cell.baseKey) : nullptr;
        const int ids[2] = {kart ? kart->abilityId[0] : -1, kart ? kart->abilityId[1] : -1};
        const int percents[2] = {kart ? static_cast<int>(kart->abilityPercent[0]) : 0, kart ? static_cast<int>(kart->abilityPercent[1]) : 0};
        drawAbilityPairs(ctx, assets, kPairsX, kPairsY, ids, percents);
        if (cell.equipped) {
            const Texture* inUse = assets.texture("Garage/UI_Shop_itembox_used.png");
            if (inUse && inUse->valid()) ctx.batch.draw(inUse->handle, kInUseX, kInUseY, static_cast<float>(inUse->width), static_cast<float>(inUse->height));
        }
    }
    char badge[48];
    std::snprintf(badge, sizeof(badge), "Icon/lv_icon_%03d.png", std::min(std::max(static_cast<int>(cell.requiredLevel), 1), 50));
    const Texture* level = assets.texture(badge);
    if (level && level->valid()) ctx.batch.draw(level->handle, 542.f, kBigY, static_cast<float>(level->width), static_cast<float>(level->height));
    drawAligned(ctx, ctx.bold, cell.label, kNameX, kNameY, 16.f, kInkDark, Align::Centre);
    const Catalog& cat = m_app.session().catalog();
    const std::vector<CarCraftPreset>& presets = cat.carCraftPresets();
    if (kartRow && cell.preset >= 0 && cell.preset < static_cast<int>(presets.size())) {
        // 0x415FF0 a factory slot lists its chassis then the seven installed parts in two columns of 160
        std::vector<std::string> names = {m_app.tr(kartDef->nameKey)};
        for (uint32_t instance : presets[static_cast<size_t>(cell.preset)].slot) {
            const CarCraftPartInstance* part = instance ? cat.carCraftInstance(instance) : nullptr;
            const CarCraftPartDef* def = part ? cat.carCraftPart(part->partKey) : nullptr;
            if (def) names.push_back(m_app.tr(def->nameKey));
        }
        for (size_t i = 0; i < names.size(); ++i) {
            const float x = kDescX + 160.f * static_cast<float>(i % 2);
            const float y = kDescY + 18.f * static_cast<float>(i / 2);
            ctx.font.draw(ctx.batch, names[i], x, y, 14.f, kInkDark);
        }
    } else {
        const std::vector<std::string> lines = wrapText(ctx.font, m_app.tr(cell.descKey), 15.f, kDescW);
        float y = kDescY;
        for (const std::string& line : lines) {
            if (y > kDescY + 90.f) break;
            ctx.font.draw(ctx.batch, line, kDescX, y, 15.f, kInkDark);
            y += 18.f;
        }
    }
    if (kartRow) {
        const std::array<float, 4> bars = m_app.session().catalog().statBars(*kartDef);
        for (int i = 0; i < 4; ++i) {
            const float target = bars[static_cast<size_t>(i)];
            if (m_barShown[i] < target) m_barShown[i] = std::min(target, m_barShown[i] + 8.f);
            else m_barShown[i] = target;
        }
        drawKartGraph(ctx, assets, kGraphX, kGraphY, m_barShown);
    }
}

}
