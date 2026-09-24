#include "ShopScreen.h"

#include "app/App.h"
#include "assets/AssetStore.h"
#include "engine/render/scene_renderer.h"
#include "net/Utf.h"
#include "screens/GarageScreen.h"
#include "ui/MenuFrame.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace KnC::Client {

namespace {

// stock shop class sub 417F40 the top bar at 25 50 the back at 37 88 on the frame
constexpr float kTopX = 25.f;
constexpr float kTopY = 50.f;
constexpr float kBackX = 37.f;
constexpr float kBackY = 88.f;
constexpr float kBackW = 942.f;
constexpr float kBackH = 629.f;
// the tiles three columns of 169 two rows of 228 from 444 191 the thumbnail back is 157 by 220
constexpr float kGridX = 444.f;
constexpr float kGridY = 191.f;
constexpr float kCellStepX = 169.f;
constexpr float kCellStepY = 228.f;
constexpr float kTileW = 157.f;
constexpr float kTileH = 220.f;
constexpr int kCols = 3;
constexpr int kRows = 2;
constexpr Rect kPageLeft = {628.f, 650.f, 24.f, 24.f};
constexpr Rect kPageRight = {729.f, 650.f, 24.f, 24.f};
constexpr float kDurX = 67.f;
constexpr float kDurY = 516.f;
// the picture after a buy waits this long for the ack
constexpr float kBuyCaptureDelay = 0.8f;
// the ShopItem popup at 252 110 its rows of 38 px from plus 318 the select bar from plus 96
constexpr float kPopX = 252.f;
constexpr float kPopY = 110.f;
constexpr float kPopRowX = 96.f;
constexpr float kPopRowY = 318.f;
constexpr float kPopRowStep = 38.f;

// the five JSON category buttons char car item roomcraft carcraft
const char* const kCategoryButtons[5] = {"button_6", "button_7", "button_8", "button_9", "button_10"};
const char* const kCharSubButtons[6] = {"button_11", "button_12", "button_13", "button_14", "button_15", "button_16"};
const char* const kKartSubButtons[4] = {"button_17", "button_18", "button_19", "button_20"};
// the room craft and car craft sub tabs of the JSON five room parts then eight car parts
const char* const kRoomSubButtons[5] = {"button_22", "button_23", "button_24", "button_25", "button_26"};
const char* const kCraftSubButtons[8] = {"button_27", "button_28", "button_29", "button_30", "button_31", "button_32", "button_33", "button_34"};

}

void ShopScreen::enter() {
    loadLayout("ui_state_13_shop.json");
    AssetStore& assets = m_app.assets();
    // the JSON parks its back top badge and buy sprites at the origin the screen draws them itself
    for (int i = 0; i <= 3; ++i) if (Widget* w = find("image_" + std::to_string(i))) w->visible = false;
    if (Widget* w = find("image_35")) w->visible = false;
    for (int i = 0; i < 5; ++i) if (Widget* w = find(kCategoryButtons[i])) w->action = "cat" + std::to_string(i);
    for (int i = 0; i < 6; ++i) if (Widget* w = find(kCharSubButtons[i])) w->action = "sub" + std::to_string(i);
    for (int i = 0; i < 4; ++i) if (Widget* w = find(kKartSubButtons[i])) w->action = "sub" + std::to_string(i);
    if (Widget* w = find("button_21")) w->action = "sub0";
    for (int i = 0; i < 5; ++i) if (Widget* w = find(kRoomSubButtons[i])) w->action = "sub" + std::to_string(i);
    for (int i = 0; i < 8; ++i) if (Widget* w = find(kCraftSubButtons[i])) w->action = "sub" + std::to_string(i);
    addMenuFrame(*this, assets, FrameMode::Full, "shop");
    addCharPanelWidgets(*this, assets);
    m_users = UserListState();
    hookUserListTap(m_app, &m_users);
    m_orbit = 0.f;

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
    button("btn_page_left", "page_left", "Buttons/Common_Page_Left_", kPageLeft);
    button("btn_page_right", "page_right", "Buttons/Common_Page_Right_", kPageRight);

    m_time = 0.f;
    m_capturedBuy = false;
    m_boughtAt = -1.f;
    m_status.clear();
    const Profile& p = m_app.session().profile();
    m_goldBefore = p.gold;
    m_astroBefore = p.astro;
    std::printf("[shop] wallet before %u gold %u astro\n", m_goldBefore, m_astroBefore);
    m_step = 0;
    m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
    // the stock opens on the character tab
    selectTab(0, 0);
}

void ShopScreen::leave() {
    m_app.setFrameTap(nullptr);
    KnC::Render::ViewportRect full;
    full.x = 0;
    full.y = 0;
    full.width = m_app.width();
    full.height = m_app.height();
    m_app.renderer().set_viewport(full);
}

void ShopScreen::selectTab(int category, int sub) {
    m_category = category;
    m_sub = sub;
    m_page = 0;
    m_selected = -1;
    m_priceIndex = 0;
    refreshTiles();
    refreshButtons();
    refreshPreview();
}

void ShopScreen::refreshButtons() {
    for (int i = 0; i < 5; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kCategoryButtons[i])) b->active = i == m_category;
    for (int i = 0; i < 6; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kCharSubButtons[i])) b->active = m_category == 0 && i == m_sub;
    for (int i = 0; i < 4; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kKartSubButtons[i])) b->active = m_category == 1 && i == m_sub;
    if (ButtonWidget* b = findAs<ButtonWidget>("button_21")) b->active = m_category == 2;
    for (int i = 0; i < 5; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kRoomSubButtons[i])) b->active = m_category == 3 && i == m_sub;
    for (int i = 0; i < 8; ++i) if (ButtonWidget* b = findAs<ButtonWidget>(kCraftSubButtons[i])) b->active = m_category == 4 && i == m_sub;
    for (int i = 0; i < 6; ++i) if (Widget* w = find(kCharSubButtons[i])) w->visible = m_category == 0;
    for (int i = 0; i < 4; ++i) if (Widget* w = find(kKartSubButtons[i])) w->visible = m_category == 1;
    if (Widget* w = find("button_21")) w->visible = m_category == 2;
    for (int i = 0; i < 5; ++i) if (Widget* w = find(kRoomSubButtons[i])) w->visible = m_category == 3;
    for (int i = 0; i < 8; ++i) if (Widget* w = find(kCraftSubButtons[i])) w->visible = m_category == 4;
}

void ShopScreen::refreshTiles() {
    m_tiles.clear();
    Session& session = m_app.session();
    AssetStore& assets = m_app.assets();
    const Catalog& cat = session.catalog();
    const uint32_t level = session.profile().level;
    const uint32_t myDriver = session.myDriverKey();
    const uint32_t myKart = session.myKartKey();

    auto pushParts = [&](int category, int sub) {
        for (const PartRow& row : cat.parts()) {
            if (!row.visible) continue;
            if (!partInTab(row, category, sub)) continue;
            if (!partAllowed(row, myDriver, myKart)) continue;
            if (row.requiredLevel > level) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::Part);
            t.baseKey = row.key;
            t.requiredLevel = row.requiredLevel;
            t.badge = row.badge;
            t.label = m_app.tr(row.nameKey);
            t.icon = partIcon(row);
            t.bigIcon = partIcon(row, true);
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            for (const OwnedPart& owned : cat.ownedParts()) if (owned.partKey == row.key) t.owned = true;
            m_tiles.push_back(t);
        }
    };

    // FUN 0041A3C0 walks the 0x0108 rows and keeps the ones whose category is the sub tab
    auto pushCraftParts = [&](int category) {
        for (const CarCraftPartDef& row : cat.carCraftParts()) {
            if (row.enabled == 0 || static_cast<int>(row.category) != category) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::CarCraft);
            t.baseKey = row.key;
            t.label = m_app.tr(row.nameKey);
            t.icon = carCraftIcon(row.model, category);
            t.bigIcon = carCraftIcon(row.model, category, true);
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            for (const CarCraftPartInstance& owned : cat.carCraftInstances()) if (owned.partKey == row.key) t.owned = true;
            m_tiles.push_back(t);
        }
    };

    if (m_category == 0 && m_sub == 0) {
        for (const DriverRow& row : cat.drivers()) {
            if (!row.visible || row.requiredLevel > level) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::Character);
            t.baseKey = row.key;
            t.requiredLevel = row.requiredLevel;
            t.badge = row.badge;
            t.label = m_app.tr(row.nameKey);
            t.icon = driverIcon(row);
            t.bigIcon = driverIcon(row, true);
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            t.previewDriver = row.asset;
            for (const OwnedCharacter& owned : cat.ownedCharacters()) if (owned.driverKey == row.key) t.owned = true;
            m_tiles.push_back(t);
        }
    } else if (m_category == 0 && m_sub == 5) {
        for (const PetRow& row : cat.pets()) {
            if (!row.visible) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::Pet);
            t.baseKey = row.key;
            t.badge = row.badge;
            t.label = m_app.tr(row.nameKey);
            t.icon = petIcon(row);
            t.bigIcon = petIcon(row, true);
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            for (const OwnedPet& owned : cat.ownedPets()) if (owned.petKey == row.key) t.owned = true;
            m_tiles.push_back(t);
        }
    } else if (m_category == 0) {
        pushParts(0, m_sub);
    } else if (m_category == 1 && m_sub == 0) {
        for (const KartRow& row : cat.karts()) {
            if (!row.visible || row.requiredLevel > level) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::Kart);
            t.baseKey = row.key;
            t.requiredLevel = row.requiredLevel;
            t.badge = row.badge;
            t.label = m_app.tr(row.nameKey);
            t.icon = kartIcon(row);
            t.bigIcon = kartIcon(row, true);
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            t.previewKart = row.model;
            for (const OwnedKart& owned : cat.ownedKarts()) if (owned.kartKey == row.key) t.owned = true;
            m_tiles.push_back(t);
        }
    } else if (m_category == 1) {
        pushParts(1, m_sub);
    } else if (m_category == 2) {
        for (const ItemRow& row : cat.items()) {
            if (!row.visible || row.requiredLevel > level) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::Consumable);
            t.baseKey = row.key;
            t.requiredLevel = row.requiredLevel;
            t.badge = row.badge;
            t.label = m_app.tr(row.nameKey);
            t.icon = pickIcon(assets, itemIcon(row), "Parts/item_" + row.nameKey + "_01.png");
            t.bigIcon = pickIcon(assets, itemIcon(row, true), t.icon);
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            for (const OwnedItem& owned : cat.ownedItems()) if (owned.itemKey == row.key) t.owned = true;
            m_tiles.push_back(t);
        }
    } else if (m_category == 3) {
        // sub 41A160 walks the 0x010C rows and keeps the ones whose category is the sub tab
        const int wanted = roomCraftCategoryOf(m_sub);
        for (const RoomObjectRow& row : cat.roomObjects()) {
            if (!row.visible || static_cast<int>(row.category) != wanted) continue;
            ShopTile t;
            t.category = static_cast<uint32_t>(BuyCategory::RoomCraft);
            t.baseKey = row.key;
            t.badge = row.badge;
            t.label = m_app.tr(row.nameKey);
            // the room object icon is the folder name under Image Parts like every other row
            t.icon = "Parts/" + row.folder + "_01.png";
            t.bigIcon = "Parts/" + row.folder + "_03.png";
            t.descKey = row.descKey;
            t.quotes = quotesOf(cat, row.prices);
            m_tiles.push_back(t);
        }
    } else if (m_category == 4) {
        const int wanted = carCraftCategoryOf(m_sub);
        if (wanted < 0) {
            // FUN 0041A630 lists the karts whose model scheme is not zero the factory cars
            for (const KartRow& row : cat.karts()) {
                if (!row.visible || row.modelScheme == 0 || row.requiredLevel > level) continue;
                ShopTile t;
                t.category = static_cast<uint32_t>(BuyCategory::Kart);
                t.baseKey = row.key;
                t.requiredLevel = row.requiredLevel;
                t.badge = row.badge;
                t.label = m_app.tr(row.nameKey);
                t.icon = kartIcon(row);
                t.bigIcon = kartIcon(row, true);
                t.descKey = row.descKey;
                t.quotes = quotesOf(cat, row.prices);
                t.previewKart = row.model;
                for (const OwnedKart& owned : cat.ownedKarts()) if (owned.kartKey == row.key) t.owned = true;
                m_tiles.push_back(t);
            }
        } else {
            pushCraftParts(wanted);
        }
    }
    // shop tile draw 0x419CC0 and 0x4199D0 skip rows with no 0x00C6 price row so no def shows an empty price
    std::string skipped;
    m_tiles.erase(std::remove_if(m_tiles.begin(), m_tiles.end(),
                                 [&](const ShopTile& t) {
                                     const bool drop = t.quotes.empty() || !t.quotes.front().resolved;
                                     if (drop) skipped += " " + std::to_string(t.baseKey);
                                     return drop;
                                 }),
                  m_tiles.end());
    std::printf("[shop] tab %d sub %d lists %zu tiles no price row skipped%s\n", m_category, m_sub, m_tiles.size(),
                skipped.empty() ? " none" : skipped.c_str());
    if (m_selected >= static_cast<int>(m_tiles.size())) m_selected = -1;
    if (m_selected < 0 && !m_tiles.empty()) m_selected = 0;
}

// the picked kart or driver replaces the own one on the preview the rest shows what the player wears
void ShopScreen::refreshPreview() {
    if (!m_sceneReady) return;
    const Session& session = m_app.session();
    const KartRow* myKart = session.catalog().kart(session.myKartKey());
    const DriverRow* myDriver = session.catalog().driver(session.myDriverKey());
    std::string model = myKart && !myKart->model.empty() ? myKart->model : std::string("Basic_1");
    std::string asset = myDriver && !myDriver->asset.empty() ? myDriver->asset : std::string("Cosmo");
    if (m_selected >= 0 && m_selected < static_cast<int>(m_tiles.size())) {
        const ShopTile& t = m_tiles[static_cast<size_t>(m_selected)];
        if (!t.previewKart.empty()) model = t.previewKart;
        if (!t.previewDriver.empty()) asset = t.previewDriver;
    }
    if (model == m_previewKart && asset == m_previewDriver && m_previewCar >= 0) return;
    if (m_previewCar >= 0) m_view.removeCar(m_previewCar);
    m_previewKart = model;
    m_previewDriver = asset;
    // the paint goes in with the car so the first drawn frame swaps no body and uploads nothing
    m_previewCar = m_view.addCar(m_app.renderer(), m_app.options().gameDir, model, asset, charPreviewPaint(m_app, model));
    CarPose pose;
    m_view.setPose(m_previewCar, pose, 0.f, 0.f);
}

Rect ShopScreen::tileRect(size_t index) const {
    const int column = static_cast<int>(index) % kCols;
    const int row = static_cast<int>(index) / kCols;
    return {kGridX + static_cast<float>(column) * kCellStepX, kGridY + static_cast<float>(row) * kCellStepY, kTileW, kTileH};
}

// one staged step per second the tab change and its picture never share a frame
void ShopScreen::update(float dt) {
    m_time += dt;
    m_orbit = turnCharPreview(*this, m_orbit, dt);
    if (!m_app.captureMode() || m_app.scripted()) return;
    const bool wantBuy = m_app.options().autoBuy;
    const float due = static_cast<float>(m_step) * 0.8f + 1.2f;
    if (m_boughtAt >= 0.f) {
        if (!m_capturedBuy && m_time - m_boughtAt > kBuyCaptureDelay) {
            m_capturedBuy = true;
            m_app.captureStage("shop_buy");
            if (m_app.options().stopAt == "shop") m_app.finishRun();
        }
        return;
    }
    if (m_time < due) return;
    switch (m_step) {
    case 0:
        m_app.captureStage("shop");
        // the item box state opens the popup of the first tile and takes its picture
        if (m_app.options().stopAt == "shopitem") { m_selected = 0; openItemPopup(); m_app.captureStage("shopitem"); m_app.finishRun(); }
        break;
    case 1: selectTab(1, 0); break;
    case 2: m_app.captureStage("shop2"); break;
    case 3: selectTab(1, 3); break;
    case 4: m_app.captureStage("shop3"); break;
    case 5: selectTab(2, 0); break;
    case 6:
        if (wantBuy) {
            if (pickAutoBuy()) buy(m_priceIndex);
            else m_status = "auto buy found no affordable row";
        }
        if (!wantBuy && m_app.options().stopAt == "shop") m_app.finishRun();
        break;
    default: break;
    }
    ++m_step;
}

// a popup drew its own scene over the stage so the preview scene and the kart go up again
void ShopScreen::sceneLost() {
    m_sceneReady = loadCharPreviewScene(m_app, m_view, m_previewWorld);
    m_previewCar = -1;
    m_previewKart.clear();
    m_previewDriver.clear();
    refreshPreview();
}

bool ShopScreen::drawScene() {
    if (!m_sceneReady || m_previewCar < 0 || m_users.open) return false;
    drawCharPreview(m_app, m_view, m_previewCar, m_orbit);
    return true;
}

void ShopScreen::drawBack(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    const Rect hole = m_users.open ? Rect{0.f, 0.f, 0.f, 0.f} : kCharPanelHole;
    drawTextureWithHole(batch, assets.texture("Shop/Shop_Back.png"), kBackX, kBackY, hole);
    const Texture* top = assets.texture("Shop/Shop_Top.png");
    if (top && top->valid()) batch.draw(top->handle, kTopX, kTopY, static_cast<float>(top->width), static_cast<float>(top->height));
}

void ShopScreen::draw(SpriteBatch& batch) {
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    // the wallpaper and the common back keep the preview hole open the 3D lands under the sprites
    drawFrameBack(batch, m_app.assets(), m_users.open ? Rect{0.f, 0.f, 0.f, 0.f} : kCharPanelHole);
    for (Widget* w : m_order) if (w->visible && w->zIndex < 0 && w->zIndex > -19) w->draw(ctx);
    drawBack(batch);
    for (Widget* w : m_order) if (w->visible && w->zIndex >= 0) w->draw(ctx);
    drawOverlay(ctx);
}

// the cheapest option the wallet can pay so a proof run costs as little as it can
bool ShopScreen::pickAutoBuy() {
    const Profile& p = m_app.session().profile();
    uint32_t best = 0;
    int bestTile = -1;
    int bestQuote = 0;
    for (size_t i = 0; i < m_tiles.size(); ++i) {
        const ShopTile& t = m_tiles[i];
        for (size_t q = 0; q < t.quotes.size(); ++q) {
            const uint32_t cost = t.quotes[q].charged();
            if (!t.quotes[q].resolved || cost == 0 || cost > p.gold) continue;
            if (bestTile >= 0 && cost >= best) continue;
            best = cost;
            bestTile = static_cast<int>(i);
            bestQuote = static_cast<int>(q);
        }
    }
    if (bestTile < 0) return false;
    m_selected = bestTile;
    m_priceIndex = bestQuote;
    return true;
}

// 0x460D53 a pet whose definition names a pendant is bought only by a holder of that pendant
static bool shopPendantGate(App& app, const ShopTile& tile) {
    if (tile.category != 4) return true;
    const PetRow* pet = app.session().catalog().pet(tile.baseKey);
    if (!pet || pet->requiredPendant == 0 || app.session().ownsPendant(pet->requiredPendant)) return true;
    std::printf("[shop] pet %u wants pendant %u MSG_NOT_CONDITION\n", tile.baseKey, pet->requiredPendant);
    app.showMessage(app.tr("MSG_NOT_CONDITION"));
    return false;
}

void ShopScreen::buy(int priceIndex) {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_tiles.size())) { m_status = "pick a row first"; return; }
    const ShopTile& tile = m_tiles[static_cast<size_t>(m_selected)];
    if (tile.quotes.empty()) { m_status = "the row carries no price option"; return; }
    if (!shopPendantGate(m_app, tile)) return;
    const size_t index = static_cast<size_t>(std::min<int>(std::max(priceIndex, 0), static_cast<int>(tile.quotes.size()) - 1));
    const PriceQuote& quote = tile.quotes[index];
    // the resolver sends minus one when the price row is missing the server refuses it
    const int32_t priceKey = quote.resolved ? static_cast<int32_t>(quote.priceKey) : -1;
    m_app.session().buy(tile.category, tile.baseKey, priceKey);
    m_boughtAt = m_time;
    m_status = "0x00B7 category " + std::to_string(tile.category) + " key " + std::to_string(tile.baseKey) +
               " price " + std::to_string(priceKey);
    std::printf("[shop] buy %s category %u key %u price %d for %u\n", tile.label.c_str(), tile.category, tile.baseKey,
                priceKey, quote.charged());
}

void ShopScreen::openItemPopup(bool fromClick) {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_tiles.size())) return;
    m_app.pushScreen(std::make_unique<ShopItemPopup>(m_app, *this, m_tiles[static_cast<size_t>(m_selected)],
                                                    ShopItemPopup::Mode::Buy, fromClick));
}

// the tile gift button opens the same box in the gift mode of sub 417A20
void ShopScreen::openGiftPopup(bool fromClick) {
    if (m_selected < 0 || m_selected >= static_cast<int>(m_tiles.size())) return;
    m_app.pushScreen(std::make_unique<ShopItemPopup>(m_app, *this, m_tiles[static_cast<size_t>(m_selected)],
                                                    ShopItemPopup::Mode::Gift, fromClick));
}

void ShopScreen::exitShop() {
    m_status = "leaving 0x0012";
    m_app.session().openLobby();
}

void ShopScreen::onKey(int key, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) { exitShop(); return; }
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) { openItemPopup(); return; }
        if (key == GLFW_KEY_TAB) { selectTab((m_category + 1) % 3, 0); return; }
        if (key == GLFW_KEY_LEFT && m_selected > 0) { --m_selected; m_priceIndex = 0; refreshPreview(); return; }
        if (key == GLFW_KEY_RIGHT && m_selected + 1 < static_cast<int>(m_tiles.size())) { ++m_selected; m_priceIndex = 0; refreshPreview(); return; }
    }
    WidgetScreen::onKey(key, action, mods);
}

void ShopScreen::onMouseButton(int button, int action, float x, float y) {
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
        const size_t perPage = static_cast<size_t>(kCols * kRows);
        const size_t first = static_cast<size_t>(m_page) * perPage;
        for (size_t i = 0; i < perPage; ++i) {
            const size_t index = first + i;
            if (index >= m_tiles.size()) break;
            const Rect r = tileRect(i);
            if (!r.contains(x, y)) continue;
            const ShopTile& tile = m_tiles[index];
            m_selected = static_cast<int>(index);
            m_priceIndex = 0;
            refreshPreview();
            const bool pet = tile.category == static_cast<uint32_t>(BuyCategory::Pet);
            // sub 4186D0 gift rect at plus 9 buy rect at plus 79 on 184 both need the level
            const bool locked = tile.requiredLevel > m_app.session().profile().level;
            const Rect buyBtn = {r.x + (pet ? 42.f : 79.f), r.y + 184.f, 70.f, 27.f};
            const Rect giftBtn = {r.x + 9.f, r.y + 184.f, 70.f, 27.f};
            if (!locked && buyBtn.contains(x, y)) { m_app.click(); openItemPopup(true); }
            else if (!locked && !pet && giftBtn.contains(x, y)) { m_app.click(); openGiftPopup(true); }
            return;
        }
    }
    WidgetScreen::onMouseButton(button, action, x, y);
}

void ShopScreen::onAction(const std::string& passedAction, Widget& source) {
    // loadLayout caught the button action when the JSON had none so read the live one enter set on the tab
    const std::string& action = source.action.empty() ? passedAction : source.action;
    if (action.rfind("cat", 0) == 0) { selectTab(action[3] - '0', 0); return; }
    if (action.rfind("sub", 0) == 0) { selectTab(m_category, action[3] - '0'); return; }
    if (action == "lobby" || action == "quit") { exitShop(); return; }
    if (action == "garage") { m_app.session().openGarage(); m_status = "garage 0x000F sent"; return; }
    if (charPanelAction(*this, m_app, m_users, action, &m_orbit)) return;
    if (action == "page_left") { if (m_page > 0) --m_page; return; }
    if (action == "page_right") {
        const int pages = (static_cast<int>(m_tiles.size()) + kCols * kRows - 1) / (kCols * kRows);
        if (m_page + 1 < pages) ++m_page;
        return;
    }
    if (frameAction(m_app, action)) return;
    m_status = source.id + " is not part of this phase";
}

void ShopScreen::onSession(SessionEvent event) {
    if (event == SessionEvent::GiftOk) {
        const Profile& p = m_app.session().profile();
        std::printf("[shop] wallet after the gift %u gold %u astro\n", p.gold, p.astro);
        m_status = "gift sent, gold " + std::to_string(p.gold) + "  astro " + std::to_string(p.astro);
    }
    if (event == SessionEvent::BuyOk) {
        const Profile& p = m_app.session().profile();
        std::printf("[shop] wallet after %u gold %u astro\n", p.gold, p.astro);
        m_status = "bought, gold " + std::to_string(m_goldBefore) + " to " + std::to_string(p.gold) +
                   "  astro " + std::to_string(m_astroBefore) + " to " + std::to_string(p.astro);
        refreshTiles();
    } else if (event == SessionEvent::InventoryChanged) {
        refreshTiles();
    }
}

void ShopScreen::drawOverlay(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    const Session& session = m_app.session();
    const OwnedKart* kart = nullptr;
    const int32_t instance = session.profile().kartInstance;
    if (instance >= 0) kart = session.catalog().ownedKart(static_cast<uint32_t>(instance));
    drawCharPanel(ctx, assets, m_users, &m_app.session());
    if (!m_users.open && kart && kart->expiryKind == 3)
        drawDurability(ctx, assets, kDurX, kDurY, kart->durability, kartDurabilityMax(session.catalog(), *kart));
    drawCharInfo(ctx, assets, session.profile());
    drawGrid(ctx);
    if (!m_status.empty() && m_app.options().statusLine) ctx.font.draw(ctx.batch, m_status, 444.f, 690.f, 12.f, kInkGrey);
}

// shop tile draw 0x4199D0 back 444 191 icon plus 13 10 badge 108 13 gift 9 184 buy 79 184
void ShopScreen::drawGrid(DrawContext& ctx) {
    AssetStore& assets = m_app.assets();
    const uint32_t level = m_app.session().profile().level;
    const Texture* box = assets.texture("Shop/Shop_Thumbnail_Back_00.png");
    const Texture* boxOn = assets.texture("Shop/Shop_Thumbnail_Back_01.png");
    const Texture* isNew = assets.texture("Shop/UI_Shop_itembox_new.png");
    const Texture* isHot = assets.texture("Shop/UI_Shop_itembox_hot.png");
    // the bought overlay is UI Shop itembox buy the Eng art reads Purchased
    const Texture* used = assets.texture("Shop/UI_Shop_itembox_buy.png");
    const Texture* buyArt = assets.texture("Shop/Shop_Thumbnail_Buy_00.png");
    const Texture* buyLocked = assets.texture("Shop/Shop_Thumbnail_Buy_03.png");
    const Texture* giftArt = assets.texture("Shop/Shop_Thumbnail_gift_00.png");
    const Texture* giftLocked = assets.texture("Shop/Shop_Thumbnail_gift_03.png");
    const size_t perPage = static_cast<size_t>(kCols * kRows);
    const size_t first = static_cast<size_t>(m_page) * perPage;
    for (size_t i = 0; i < perPage; ++i) {
        const size_t index = first + i;
        if (index >= m_tiles.size()) break;
        const ShopTile& tile = m_tiles[index];
        const Rect r = tileRect(i);
        const bool picked = static_cast<int>(index) == m_selected;
        const bool locked = tile.requiredLevel > level;
        const bool pet = tile.category == static_cast<uint32_t>(BuyCategory::Pet);
        const Texture* art = picked && boxOn && boxOn->valid() ? boxOn : box;
        if (art && art->valid()) ctx.batch.draw(art->handle, r.x, r.y, static_cast<float>(art->width), static_cast<float>(art->height));
        const Texture* icon = tile.bigIcon.empty() ? nullptr : assets.texture(tile.bigIcon);
        if (!icon) icon = tile.icon.empty() ? nullptr : assets.texture(tile.icon);
        if (icon && icon->valid()) ctx.batch.draw(icon->handle, r.x + 13.f, r.y + 10.f, 128.f, 128.f);
        if (tile.requiredLevel > 0) {
            char badge[48];
            std::snprintf(badge, sizeof(badge), "Icon/lv_icon_%03d.png", std::min(static_cast<int>(tile.requiredLevel), 50));
            const Texture* levelArt = assets.texture(badge);
            if (levelArt && levelArt->valid()) ctx.batch.draw(levelArt->handle, r.x + 108.f, r.y + 13.f, static_cast<float>(levelArt->width), static_cast<float>(levelArt->height));
        }
        // a kart tile wears the two ability pairs of its 0x00C0 row an id off 0 to 25 hides
        if (tile.category == static_cast<uint32_t>(BuyCategory::Kart)) {
            const KartRow* kart = m_app.session().catalog().kart(tile.baseKey);
            const int ids[2] = {kart ? kart->abilityId[0] : -1, kart ? kart->abilityId[1] : -1};
            const int percents[2] = {kart ? static_cast<int>(kart->abilityPercent[0]) : 0, kart ? static_cast<int>(kart->abilityPercent[1]) : 0};
            drawAbilityPairs(ctx, assets, r.x + 10.f, r.y + 95.f, ids, percents);
        }
        drawAligned(ctx, ctx.font, tile.label, r.x + 78.f, r.y + 149.f, 13.f, kInkWhite, Align::Centre);
        const Texture* buy = locked && buyLocked && buyLocked->valid() ? buyLocked : buyArt;
        const Texture* gift = locked && giftLocked && giftLocked->valid() ? giftLocked : giftArt;
        if (pet) {
            if (buy && buy->valid()) ctx.batch.draw(buy->handle, r.x + 42.f, r.y + 184.f, static_cast<float>(buy->width), static_cast<float>(buy->height));
        } else {
            if (gift && gift->valid()) ctx.batch.draw(gift->handle, r.x + 9.f, r.y + 184.f, static_cast<float>(gift->width), static_cast<float>(gift->height));
            if (buy && buy->valid()) ctx.batch.draw(buy->handle, r.x + 79.f, r.y + 184.f, static_cast<float>(buy->width), static_cast<float>(buy->height));
        }
        if (tile.owned && used && used->valid()) ctx.batch.draw(used->handle, r.x + 36.f, r.y + 105.f, static_cast<float>(used->width), static_cast<float>(used->height));
        const Texture* badge = tile.badge == 1 ? isNew : (tile.badge == 2 ? isHot : nullptr);
        if (badge && badge->valid()) ctx.batch.draw(badge->handle, r.x - 20.f, r.y - 10.f, static_cast<float>(badge->width), static_cast<float>(badge->height));
    }
    const int pages = std::max(1, (static_cast<int>(m_tiles.size()) + static_cast<int>(perPage) - 1) / static_cast<int>(perPage));
    drawAligned(ctx, ctx.bold, std::to_string(m_page + 1) + " / " + std::to_string(pages), 691.f, 653.f, 16.f, kInkDark, Align::Centre);
}

ShopItemPopup::ShopItemPopup(App& app, ShopScreen& shop, const ShopTile& tile, Mode mode, bool fromClick)
    : m_app(app), m_shop(&shop), m_tile(tile), mode_(mode), m_openRelease(fromClick) {}

// the garage opens the box on its own Buy button the buy then goes straight through the session
ShopItemPopup::ShopItemPopup(App& app, const ShopTile& tile, bool fromClick)
    : m_app(app), m_tile(tile), m_openRelease(fromClick) {}

namespace {
// sub 45D0E0 mode 3 name box at plus 370 190 rows at plus 230 on 210 every 36
constexpr Rect kGiftNameBox = {kPopX + 230.f, kPopY + 190.f, 140.f, 25.f};
constexpr Rect kGiftDrop = {kPopX + 370.f, kPopY + 190.f, 24.f, 25.f};
constexpr float kGiftRowX = kPopX + 230.f;
constexpr float kGiftRowY = kPopY + 210.f;
constexpr float kGiftRowW = 164.f;
constexpr float kGiftRowH = 36.f;
constexpr Rect kGiftUp = {kPopX + 395.f, kPopY + 210.f, 20.f, 20.f};
constexpr Rect kGiftDown = {kPopX + 395.f, kPopY + 294.f, 20.f, 20.f};
constexpr int kGiftRows = 3;
}

// sub 45CEF0 back 252 110 icon plus 36 60 text plus 210 87 rows of 38 from 318
void ShopItemPopup::draw(SpriteBatch& batch) {
    AssetStore& assets = m_app.assets();
    DrawContext ctx{batch, m_app.font(), m_app.fontBold()};
    auto sprite = [&](const std::string& path, float x, float y) -> const Texture* {
        const Texture* t = assets.texture(path);
        if (t && t->valid()) batch.draw(t->handle, x, y, static_cast<float>(t->width), static_cast<float>(t->height));
        return t;
    };
    if (!sprite("Popup/ShopItem/Shop_Popup_Back.png", kPopX, kPopY)) batch.fill(kPopX, kPopY, 555.f, 480.f, rgba(40, 40, 60, 240));
    // the title bar carries the name in bold centred on the box
    drawAligned(ctx, ctx.bold, m_tile.label, kPopX + 277.f, kPopY + 12.f, 15.f, kInkDark, Align::Centre);
    const Texture* icon = m_tile.bigIcon.empty() ? nullptr : assets.texture(m_tile.bigIcon);
    if (icon && icon->valid()) batch.draw(icon->handle, kPopX + 36.f, kPopY + 60.f, 128.f, 128.f);
    const std::vector<std::string> lines = wrapText(ctx.font, m_app.tr(m_tile.descKey), 13.f, 300.f);
    float y = kPopY + 87.f;
    for (const std::string& line : lines) {
        if (y > kPopY + 170.f) break;
        ctx.font.draw(batch, line, kPopX + 210.f, y, 13.f, kInkDark);
        y += 17.f;
    }
    // the price rows the currency word in orange bold the unit the price box with the amount right aligned
    for (size_t q = 0; q < m_tile.quotes.size() && q < 3; ++q) {
        const PriceQuote& quote = m_tile.quotes[q];
        const float ry = kPopY + kPopRowY + kPopRowStep * static_cast<float>(q);
        const bool picked = static_cast<int>(q) == m_priceIndex;
        sprite(picked ? "Popup/ShopItem/Shop_Popup_SelectBar_01.png" : "Popup/ShopItem/Shop_Popup_SelectBar_00.png", kPopX + kPopRowX, ry);
        ctx.bold.draw(batch, "Gold", kPopX + 41.f, ry + 13.f, 13.f, rgba(232, 140, 24, 255));
        ctx.bold.draw(batch, quote.unitText(), kPopX + 148.f, ry + 13.f, 13.f, kInkDark);
        drawAligned(ctx, ctx.bold, quote.priceText(), kPopX + 515.f, ry + 13.f, 13.f, quote.resolved ? kInkDark : kInkGrey, Align::Right);
    }
    if (m_tile.quotes.empty()) ctx.font.draw(batch, "no price option on this row", kPopX + kPopRowX, kPopY + kPopRowY, 15.f, kInkGrey);
    if (mode_ == Mode::Gift) {
        // the recipient box and the message the friend rows drop under the name box while open
        const std::vector<FriendRow>& friends = m_app.session().friends();
        batch.fill(kGiftNameBox.x, kGiftNameBox.y, kGiftNameBox.w, kGiftNameBox.h, rgba(255, 255, 255, 255));
        const std::string who = m_recipient >= 0 && m_recipient < static_cast<int>(friends.size())
                                    ? u16ToUtf8(friends[static_cast<size_t>(m_recipient)].name) : std::string("To :");
        ctx.font.draw(batch, who, kGiftNameBox.x + 4.f, kGiftNameBox.y + 5.f, 13.f, kInkDark);
        sprite(m_listOpen ? "Popup/ShopItem/UI_charinfo_subdown_01.png" : "Popup/ShopItem/UI_charinfo_subdown_00.png", kGiftDrop.x, kGiftDrop.y);
        if (m_listOpen) {
            for (int i = 0; i < kGiftRows; ++i) {
                const size_t index = static_cast<size_t>(m_listTop + i);
                if (index >= friends.size()) break;
                const float ry = kGiftRowY + kGiftRowH * static_cast<float>(i);
                batch.fill(kGiftRowX, ry, kGiftRowW, kGiftRowH - 2.f, rgba(240, 240, 250, 255));
                ctx.font.draw(batch, u16ToUtf8(friends[index].name), kGiftRowX + 6.f, ry + 10.f, 13.f, kInkDark);
            }
            if (friends.size() > static_cast<size_t>(kGiftRows)) {
                sprite("Popup/ShopItem/WaitingRoom_Chat_Up_00.png", kGiftUp.x, kGiftUp.y);
                sprite("Popup/ShopItem/WaitingRoom_Chat_Down_00.png", kGiftDown.x, kGiftDown.y);
            }
            if (friends.empty()) ctx.font.draw(batch, "no friend to gift", kGiftRowX + 6.f, kGiftRowY + 10.f, 13.f, kInkGrey);
        } else {
            // the message edit of shop buy action at plus 232 235 of 200 by 128
            ctx.font.draw(batch, m_message.empty() ? std::string("message") : m_message, kPopX + 232.f, kPopY + 240.f, 13.f,
                          m_message.empty() ? kInkGrey : kInkDark);
        }
        sprite("Buttons/Shop_Popup_Gift_half_00.png", kPopX + 184.f, kPopY + 441.f);
    } else {
        sprite("Buttons/Shop_Popup_Buy_half_00.png", kPopX + 184.f, kPopY + 441.f);
    }
    sprite("Buttons/Shop_Popup_Close_half_00.png", kPopX + 278.f, kPopY + 441.f);
}

// the buy half sends 0x00B7 the gift half sends 0x0098 with the friend name and the message
void ShopItemPopup::confirm() {
    m_app.click();
    if (mode_ == Mode::Gift) { sendGift(); return; }
    if (m_shop) { m_shop->buy(m_priceIndex); close(); return; }
    // sub 4841D0 the same verb with the price key of the picked option minus one when it did not resolve
    if (!m_tile.quotes.empty() && shopPendantGate(m_app, m_tile)) {
        const size_t index = static_cast<size_t>(std::min<int>(std::max(m_priceIndex, 0), static_cast<int>(m_tile.quotes.size()) - 1));
        const PriceQuote& quote = m_tile.quotes[index];
        m_app.session().buy(m_tile.category, m_tile.baseKey,
                            quote.resolved ? static_cast<int32_t>(quote.priceKey) : -1);
    }
    close();
}

// sub 482880 category recipient base key price key then the message through the session gift verb
void ShopItemPopup::sendGift() {
    const std::vector<FriendRow>& friends = m_app.session().friends();
    if (m_recipient < 0 || m_recipient >= static_cast<int>(friends.size())) { m_listOpen = true; return; }
    if (m_tile.quotes.empty()) return;
    const size_t index = static_cast<size_t>(std::min<int>(std::max(m_priceIndex, 0), static_cast<int>(m_tile.quotes.size()) - 1));
    const PriceQuote& quote = m_tile.quotes[index];
    m_app.session().sendGift(m_tile.category, friends[static_cast<size_t>(m_recipient)].name, m_tile.baseKey,
                             quote.resolved ? static_cast<int32_t>(quote.priceKey) : -1, utf8ToU16(m_message));
    close();
}

void ShopItemPopup::close() {
    if (m_closed) return;
    m_closed = true;
    m_app.popScreen();
}

void ShopItemPopup::onKey(int key, int action, int) {
    if (action != GLFW_PRESS) return;
    if (key == GLFW_KEY_ESCAPE) close();
    else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) confirm();
    else if (key == GLFW_KEY_BACKSPACE && mode_ == Mode::Gift && !m_message.empty()) {
        // drop one utf8 character its continuation bytes go with it
        m_message.pop_back();
        while (!m_message.empty() && (static_cast<unsigned char>(m_message.back()) & 0xC0) == 0x80) m_message.pop_back();
    }
    else if (key == GLFW_KEY_UP && m_priceIndex > 0) --m_priceIndex;
    else if (key == GLFW_KEY_DOWN && m_priceIndex + 1 < static_cast<int>(m_tile.quotes.size())) ++m_priceIndex;
}

void ShopItemPopup::onChar(unsigned codepoint) {
    if (mode_ != Mode::Gift || m_listOpen || codepoint < 32) return;
    std::u16string one;
    one.push_back(static_cast<char16_t>(codepoint));
    if (m_message.size() < 60) m_message += u16ToUtf8(one);
}

void ShopItemPopup::onMouseButton(int button, int action, float x, float y) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_RELEASE) return;
    // the tile Buy press opened the box its release arrives here a Buy under the box closed it
    if (m_openRelease) { m_openRelease = false; return; }
    for (size_t q = 0; q < m_tile.quotes.size() && q < 3; ++q) {
        const Rect row = {kPopX + 30.f, kPopY + kPopRowY + kPopRowStep * static_cast<float>(q), 500.f, 36.f};
        if (row.contains(x, y)) { m_priceIndex = static_cast<int>(q); return; }
    }
    const Rect ok = {kPopX + 184.f, kPopY + 441.f, 92.f, 28.f};
    const Rect cancel = {kPopX + 278.f, kPopY + 441.f, 92.f, 28.f};
    if (ok.contains(x, y)) { confirm(); return; }
    if (cancel.contains(x, y)) { m_app.click(); close(); return; }
    if (mode_ == Mode::Gift) {
        const int count = static_cast<int>(m_app.session().friends().size());
        if (kGiftDrop.contains(x, y) || kGiftNameBox.contains(x, y)) { m_app.click(); m_listOpen = !m_listOpen; return; }
        if (m_listOpen) {
            if (count > kGiftRows && kGiftUp.contains(x, y)) { if (m_listTop > 0) --m_listTop; return; }
            if (count > kGiftRows && kGiftDown.contains(x, y)) { if (m_listTop + kGiftRows < count) ++m_listTop; return; }
            for (int i = 0; i < kGiftRows; ++i) {
                const Rect row = {kGiftRowX, kGiftRowY + kGiftRowH * static_cast<float>(i), kGiftRowW, kGiftRowH};
                if (row.contains(x, y) && m_listTop + i < count) { m_app.click(); m_recipient = m_listTop + i; m_listOpen = false; return; }
            }
            m_listOpen = false;
            return;
        }
    }
    if (x < kPopX || x > kPopX + 555.f || y < kPopY || y > kPopY + 480.f) close();
}

}
