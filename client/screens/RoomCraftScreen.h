// the room editor stage 19 of sub 434C00 the kind tabs the owned strip the 3D field and the save
#pragma once

#include "net/Catalog.h"
#include "race/RaceView.h"
#include "ui/WidgetScreen.h"

#include <string>
#include <vector>

namespace KnC::Client {

class RoomCraftScreen : public WidgetScreen {
public:
    explicit RoomCraftScreen(App& app) : WidgetScreen(app) {}
    const char* name() const override { return "roomcraft"; }
    void enter() override;
    void leave() override;
    void update(float dt) override;
    bool drawScene() override;
    void sceneLost() override;
    void draw(SpriteBatch& batch) override;
    void onKey(int key, int action, int mods) override;
    void onMouseMove(float x, float y) override;
    void onMouseButton(int button, int action, float x, float y) override;
    void onSession(SessionEvent event) override;

protected:
    void onAction(const std::string& action, Widget& source) override;
    void drawOverlay(DrawContext& ctx) override;

private:
    // one strip tile one owned object key with the rows that carry it
    struct StripTile {
        uint32_t objectKey = 0;
        uint32_t category = 0;
        std::string folder;
        std::string nameKey;
        int available = 0;
        int placed = 0;
    };
    // one loaded nif of a placed category three row
    struct FieldProp {
        uint32_t instance = 0;
        uint32_t objectKey = 0;
        int handle = -1;
        // the bound radius of the loaded nif the marker test uses it as the COL corner run
        float radius = 0.f;
    };

    void exitToLobby();
    bool blockedPlacement(const RoomCraftInstance& row) const;
    void loadMarkers();
    void placeMarkers();
    // FUN 00433CB0 the middle drag walks the eye and the look point together inside the stage box
    void panCamera(int code);
    void takeRows();
    void refreshTiles();
    // the placed sky floor back object and effect rows as one line a change rebuilds the world
    std::string worldToken() const;
    bool loadField();
    void refreshFieldProps();
    // FUN 00435450 a strip press picks a tile to drag unless none is left or its singleton is placed
    bool canDragTile(int tile) const;
    void placeTile(int tile, const float world[3]);
    void moveSelected(const float world[3]);
    void rotateSelected(float degrees);
    void removeSelected();
    void save();
    int pickPlaced(float x, float y) const;
    bool groundPoint(float x, float y, float out[3]) const;
    bool project(const float world[3], float& sx, float& sy) const;
    int rowOfInstance(uint32_t instance) const;
    int placedOfKey(uint32_t objectKey) const;
    int placedOfCategory(uint32_t category) const;

    RaceView m_view;
    RaceWorld m_world;
    // the eye and the look point of the stage the middle drag moves the pair
    float m_eye[3] = {-105.f, 0.f, 15.f};
    float m_look[3] = {120.f, 0.f, 0.f};
    // the drag plane of World Room PICK the stock ray picks that nif when it moves an object
    float m_pickZ = 0.f;
    int m_markerOk = -1;
    int m_markerBad = -1;
    int m_markerScreen = -1;
    bool m_markersLoaded = false;
    // the working copy of the 0x010D rows the save diffs it against the catalogue master
    std::vector<RoomCraftInstance> m_rows;
    std::vector<StripTile> m_tiles;
    std::vector<FieldProp> m_fieldProps;
    std::string m_worldToken;
    std::string m_propToken;
    // 0 all 1 sky 2 terrain 3 back ground 4 object 5 effect
    int m_kind = 0;
    int m_scroll = 0;
    int m_tile = -1;
    // the strip tile the left press picked FUN 00435810 places it where the button comes up
    int m_dragTile = -1;
    int m_selected = -1;
    bool m_dragging = false;
    bool m_tip = false;
    bool m_worldReady = false;
    bool m_opened = false;
    float m_time = 0.f;
    float m_cursorX = 0.f;
    float m_cursorY = 0.f;
    bool m_captured = false;
    int m_step = 0;
    // the middle button pan anchor and the live code of FUN 00436050 message 0x200 with MK MBUTTON
    bool m_panning = false;
    float m_panX = 0.f;
    float m_panY = 0.f;
    int m_panCode = 0;
    // the right drag anchor the turn step is the pixel run times 0 1 the yaw moves twice that
    bool m_turning = false;
    float m_turnX = 0.f;
    float m_turnY = 0.f;
    std::string m_status;
};

}
