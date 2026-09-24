// the stock char column of the lobby the garage and the shop tabs frame icons and arrows
#pragma once

#include "Widgets.h"

#include "tools/track_scene/ghost_car.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace KnC::Client {

class App;
class AssetStore;
class Session;
class WidgetScreen;
class RaceView;
struct RaceWorld;

// the transparent middle of Lobby UserInfo Back 00 at 58 152 the 3D preview shows through it
constexpr Rect kCharPanelHole = {67.f, 163.f, 319.f, 379.f};
// the stock preview view rect of sub 42A050 58 110 of 338 by 462
constexpr Rect kCharPanelView = {58.f, 110.f, 338.f, 462.f};
// the two tabs of sub 4296D0 at 72 116 and 178 116
constexpr Rect kCharPanelTabUsers = {72.f, 116.f, 100.f, 30.f};
constexpr Rect kCharPanelTabInfo = {178.f, 116.f, 100.f, 30.f};

// one row of the User List tab as 0x0132 lists it
struct UserListRow {
    uint32_t playerId = 0;
    std::string name;
    int level = 1;
    int pendant = 0;
};

// the list tab state the page and the rows every stage fills it from 0x0132
struct UserListState {
    bool open = false;
    int page = 0;
    int pages = 1;
    std::vector<UserListRow> rows;
};

// the tabs the arrows the front ball and the page buttons as widgets with their actions
void addCharPanelWidgets(WidgetScreen& screen, AssetStore& assets);
// shows the widgets of one tab and hides the other ones
void showCharPanelTab(WidgetScreen& screen, AssetStore& assets, bool userList);
// the frame the corner icons and rows with a session the pendant icon of sub 429990 sits over the button
void drawCharPanel(DrawContext& ctx, AssetStore& assets, const UserListState& list, const Session* session = nullptr);
// preview scene wallpaper behind kart world outlives view
bool loadCharPreviewScene(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect = kCharPanelView);
// the stock preview camera over another sheet the texture rect is where that sheet sits on canvas
bool loadCharPreviewSceneFor(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect,
                             const std::string& texture, const Rect& textureRect);
// the same slice quad built for another camera a horizontal of zero follows the rect aspect
bool loadPreviewSceneFor(App& app, RaceView& view, RaceWorld& world, const Rect& viewRect, const float eye[3],
                         const float look[3], float fovDegrees, const std::string& texture, const Rect& textureRect,
                         float horizontalDegrees = 0.f);
// the worn look of the own character and kart as one line a stage reloads its scene when it changes
std::string charPreviewLookToken(App& app);
// the BodyColor paint of a previewed kart a stage adds its car with it so the first frame swaps nothing
std::string charPreviewPaint(App& app, const std::string& kartModel);
// what a shop tile tries on a preview a zero key or an empty model keeps what the player wears
struct PreviewTryOn {
    uint32_t petKey = 0;
    std::string paint;
    std::string plate;
    std::string antenna;
};
void setCharPreviewTryOn(const RaceView& view, const PreviewTryOn& tryOn);
// the five part keys the own character wears on that driver else the default costume of its 0x00BF row
std::array<uint32_t, 5> wornCostume(App& app, uint32_t driverKey);
// the paint plate and antenna models a kart wears the plate falls back on NAMEBOX NORMAL like the exe
struct KartLook {
    std::string paint;
    std::string plate;
    std::string antenna;
};
// car apply kart loadout 0x490A70 the part keys of a kart record win over the skins of its 0x00C0 row
KartLook kartLook(App& app, uint32_t kartKey, const std::array<uint32_t, 3>& parts);
// the 3D view kart name from a row and the 0x3C custom car block a factory token for scheme 1
std::string kartViewModel(App& app, uint32_t kartKey, const std::array<uint32_t, 15>& customCar);
// the same for an owned kart sub 450140 takes the first preset row on its instance
std::string ownedKartViewModel(App& app, uint32_t kartInstance);
// BODYSET nifs of the five part keys of one driver a key with no file is left out
std::vector<KnC::Tools::GhostDriverPart> driverParts(App& app, const std::string& asset,
                                                     const std::array<uint32_t, 5>& keys);
// the 3D preview on the view rect through the stock lens of sub 4A5ED0 the kart turns by the yaw
void drawCharPreview(App& app, RaceView& view, int carHandle, float yawDeg, const Rect& viewRect = kCharPanelView);
// the rotate arrows turn the kart while pressed
float turnCharPreview(WidgetScreen& screen, float yawDeg, float dt);
// reads every 0x0132 page off the frame tap into the state the screen clears the tap when it leaves
void hookUserListTap(App& app, UserListState* state);
// asks the server for one page of the channel user list with 0x0132
void requestUserListPage(App& app, UserListState& state, int page);
// the tab actions of the panel true when the action was one of them the front ball resets the yaw
bool charPanelAction(WidgetScreen& screen, App& app, UserListState& state, const std::string& action, float* yawDeg = nullptr);

}
