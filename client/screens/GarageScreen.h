// garage stage 6 on stock frame owned grid select verbs bars and 3D preview
#pragma once

#include "race/RaceView.h"
#include "screens/ShopCommon.h"
#include "ui/CharPanel.h"
#include "ui/WidgetScreen.h"

#include <set>
#include <string>
#include <vector>

namespace KnC::Client {

class GarageScreen : public WidgetScreen {
public:
    explicit GarageScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "garage"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    // one drawn tile of owned grid
    struct Cell {
        uint32_t baseKey = 0;
        uint32_t instance = 0;
        std::string label;
        std::string icon;
        std::string bigIcon;
        std::string descKey;
        std::string note;
        uint32_t requiredLevel = 0;
        bool equipped = false;
        // owned active flag zero row is dead and shows Delete
        bool active = true;
        // 0x0107 row of a built factory slot minus one for a catalogue kart
        int preset = -1;
    };

    void selectTab(int category, int sub);
    void refreshCells();
    void refreshButtons();
    void install();
    void remove();
    void deleteRow();
    void buyRow();
    // sub 414630 Buy slot shows on a row still sold not running forever
    bool canExtend(const Cell& cell) const;
    void exitGarage();
    void refreshPreview();
    uint32_t tabCategory() const;
    const OwnedKart* selectedKart() const;
    const OwnedCharacter* selectedCharacter() const;
    Rect cellRect(size_t index) const;
    // back art in four slices so 3D preview keeps its hole
    void drawBack(SpriteBatch& batch);
    void drawGrid(DrawContext& ctx);
    void drawDetail(DrawContext& ctx);

    RaceView m_view;
    // preview world view keeps a pointer into it so it lives with stage
    RaceWorld m_previewWorld;
    UserListState m_users;
    std::vector<Cell> m_cells;
    // base keys sent to 0x00B8 whose Delete button waits for ack
    std::set<uint32_t> m_deleted;
    int m_category = 1;
    int m_sub = 0;
    int m_page = 0;
    int m_selected = -1;
    int m_previewCar = -1;
    std::string m_previewKart;
    std::string m_previewDriver;
    // worn parts of own character and kart a change reloads preview scene
    std::string m_lookToken;
    // kart yaw of preview arrows turn it front ball resets it
    float m_orbit = 0.f;
    float m_time = 0.f;
    bool m_sceneReady = false;
    // true while repair scroll confirm box is up second install call then sends
    bool m_confirmRepair = false;
    int m_step = 0;
    // stat bars grow eight percent a frame toward their value as stock graph does
    float m_barShown[4] = {0.f, 0.f, 0.f, 0.f};
    std::string m_status;
};

// stock kart graph Common Car Info Back with its four bars at x y and durability gauge
void drawKartGraph(DrawContext& ctx, AssetStore& assets, float x, float y, const float* bars);
void drawDurability(DrawContext& ctx, AssetStore& assets, float x, float y, int durability, int maximum);
// kart durability bar draw 0x42AD20 divides by the amount of option zero of the 0x00C0 kart row
int kartDurabilityMax(const Catalog& cat, const OwnedKart& kart);
// kart ability pairs draw 0x42B910 two reg icons 76 apart the percent at plus 42 26
void drawAbilityPairs(DrawContext& ctx, AssetStore& assets, float x, float y, const int* ids, const int* percents);

}
