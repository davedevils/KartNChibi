// the shop stage 7 on the stock frame the tiles the 3D preview the item popup and the buy verb
#pragma once

#include "race/RaceView.h"
#include "screens/ShopCommon.h"
#include "ui/CharPanel.h"
#include "ui/WidgetScreen.h"

#include <string>
#include <vector>

namespace KnC::Client {

// one catalogue row as the shop grid draws it
struct ShopTile {
    uint32_t category = 0;
    uint32_t baseKey = 0;
    uint32_t requiredLevel = 0;
    uint32_t badge = 0;
    std::string label;
    std::string icon;
    std::string bigIcon;
    std::string descKey;
    std::vector<PriceQuote> quotes;
    bool owned = false;
    // the kart model or driver asset the preview swaps in empty keeps the own one
    std::string previewKart;
    std::string previewDriver;
};

class ShopScreen : public WidgetScreen {
public:
    explicit ShopScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "shop"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;
    // the popup buys the picked option of the picked tile
    void buy(int priceIndex);

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    void selectTab(int category, int sub);
    void refreshTiles();
    void refreshButtons();
    void refreshPreview();
    void openItemPopup(bool fromClick = false);
    void openGiftPopup(bool fromClick = false);
    void exitShop();
    bool pickAutoBuy();
    Rect tileRect(size_t index) const;
    void drawBack(SpriteBatch& batch);
    void drawGrid(DrawContext& ctx);

    RaceView m_view;
    // the preview world the view keeps a pointer into it so it lives with the stage
    RaceWorld m_previewWorld;
    UserListState m_users;
    int m_previewCar = -1;
    std::string m_previewKart;
    std::string m_previewDriver;
    bool m_sceneReady = false;
    // the kart yaw of the preview the arrows turn it the front ball resets it
    float m_orbit = 0.f;
    std::vector<ShopTile> m_tiles;
    int m_category = 0;
    int m_sub = 0;
    int m_page = 0;
    int m_selected = -1;
    int m_priceIndex = 0;
    float m_time = 0.f;
    int m_step = 0;
    float m_boughtAt = -1.f;
    bool m_capturedBuy = false;
    uint32_t m_goldBefore = 0;
    uint32_t m_astroBefore = 0;
    std::string m_status;
};

// the stock ShopItem popup sub 45D7B0 mode 1 buys with 0x00B7 mode 3 gifts a friend with 0x0098
class ShopItemPopup : public Screen {
public:
    enum class Mode { Buy, Gift };
    ShopItemPopup(App& app, ShopScreen& shop, const ShopTile& tile, Mode mode = Mode::Buy,
                  bool fromClick = false);
    // the garage Buy button opens the same box with no shop stage behind it
    ShopItemPopup(App& app, const ShopTile& tile, bool fromClick = false);
    const char* name() const override { return mode_ == Mode::Gift ? "shopgift" : "shopitem"; }
    bool opaque() const override { return false; }
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseButton(int button, int action, float x, float y) override;

private:
    void close();
    void confirm();
    void sendGift();
    App& m_app;
    ShopScreen* m_shop = nullptr;
    ShopTile m_tile;
    Mode mode_ = Mode::Buy;
    int m_priceIndex = 0;
    bool m_closed = false;
    // the release of the tile click that opened the box lands here first and must not close it
    bool m_openRelease = false;
    // the gift side the friend list drops down under the name box the message types under it
    bool m_listOpen = false;
    int m_listTop = 0;
    int m_recipient = -1;
    std::string m_message;
};

}
