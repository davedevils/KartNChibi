// car factory stage 18 of sub 430EF0 preset strip slot tabs part list and save
#pragma once

#include "net/Catalog.h"
#include "net/Session.h"
#include "race/RaceView.h"
#include "ui/WidgetScreen.h"

#include <string>
#include <vector>

namespace KnC::Client {

class CarCraftScreen : public WidgetScreen {
public:
    explicit CarCraftScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "carcraft"; }
    void enter() override;
    void update(float dt) override;
    void leave() override;
    bool drawScene() override;
    void sceneLost() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onChar(unsigned codepoint) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    // one row of left list owned chassis on chassis tab else one owned car craft part instance
    struct ListRow {
        uint32_t instance = 0;
        uint32_t key = 0;
        std::string label;
        std::string icon;
        int grade = 0;
        bool installed = false;
        // sub 42FE20 also draws the in use art on a row another slot holds
        bool inUse = false;
        // 0 permanent 1 days 2 times 3 durability read off 0x0109 plus 0x14 or the 0x001C row
        uint32_t periodType = 0;
        int32_t periodValue = 0;
        bool active = true;
        // built slots holding the part the row prints it as plus N
        int count = 0;
    };

    void exitToLobby();
    // FUN 004A5ED0 with factory flag chassis body then one nif per installed slot
    bool loadPreview();
    void refreshPreview();
    // installed rows as one line a change rebuilds built car
    std::string previewToken() const;
    // texture set for a grade basic under five unique under twenty epic under sixty five
    static const char* gradeFolder(int grade);
    // FUN 00454A10 period words of a row UNIT PERMANENT days times or durability
    std::string periodText(uint32_t type, int32_t value) const;
    void selectPreset(int index);
    void refreshRows();
    void install();
    void removePart();
    void save();
    void beginRename();
    void submitRename();
    // config slot for a 0x0108 category cover booster tires fenders bumper wing
    static int slotOfCategory(int category);
    // part tab of stage 0 chassis else slot type plus one
    int tabCategory() const;
    const CarCraftPreset* preset() const;

    // one appended prop per built part wheel carries O WHEEL dummy of chassis body
    struct PreviewPart {
        int handle = -1;
        bool hasLocal = false;
        float local[16] = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f};
    };

    RaceView m_view;
    RaceWorld m_previewWorld;
    std::vector<PreviewPart> m_parts;
    // plain kart for a preset whose chassis is not a factory car char preview car visual
    int m_previewCar = -1;
    std::string m_previewToken;
    bool m_sceneReady = false;
    float m_yaw = 0.f;
    CarConfig m_config;
    std::vector<ListRow> m_rows;
    // picked part tab 0 chassis 1 cover 2 booster 3 tire 4 and 5 fenders 6 bumper 7 spoiler
    int m_part = 0;
    int m_preset = 0;
    int m_presetScroll = 0;
    int m_scroll = 0;
    int m_selected = -1;
    bool m_dirty = false;
    bool m_opened = false;
    bool m_renaming = false;
    std::string m_rename;
    float m_time = 0.f;
    bool m_captured = false;
    std::string m_status;
};

}
