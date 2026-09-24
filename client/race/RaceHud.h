// race hud sprites over scene laid out as stock game draws it at 1024x768
#pragma once

#include "RaceSession.h"
#include "TrackData.h"
#include "ui/Widgets.h"

#include <string>
#include <vector>

namespace KnC::Client {

class AssetStore;

// one standings line name and zero based position
struct HudStanding {
    std::string name;
    // Driver Body High folder name face icon of Panel MiniMap named by it
    std::string driverAsset;
    int position = -1;
    int pingMs = 0;
    uint32_t team = 0;
    bool local = false;
    bool finished = false;
};

// one car dot on minimap
struct HudCar {
    float x = 0.f;
    float y = 0.f;
    std::string driverAsset;
    bool local = false;
};

// name tag over another kart in canvas units stock shows racer name above car
struct HudNameTag {
    float x = 0.f;
    float y = 0.f;
    std::string name;
};

// one row of result board as 0x0046 board names it
struct HudResultRow {
    std::string name;
    std::string driverAsset;
    int rank = 0;
    int timeMs = 0;
    uint32_t gold = 0, exp = 0, goldBonus = 0, expBonus = 0;
    bool local = false;
};

// panel set per stage panel manager 0x4A8D80 picks by stage number
enum class HudKind {
    // stage 11 every panel
    Race,
    // stage 13 minimap gauge item slot key guide timer and instruction strip
    Licence,
    // stage 15 minimap lap panel gauge and item slot lap words slide in
    Ghost,
    // stage 25 mission minimap gauge item slot and time left at 400 50
    Mission
};

// hud state screen fills each frame
struct HudState {
    HudKind kind = HudKind::Race;
    // stage 13 six key indicators up right down left shift ctrl from FUN 0041B600 1 while held
    bool licenceKeys[6] = {false, false, false, false, false, false};
    // stage 13 time limit left in ms drawn at 400 50 once test runs below zero hides it
    double licenceTimerMs = -1.0;
    // stage 13 LicenseGame text strip centred on 529 29 empty draws none
    std::string licenceLine;
    // stage 15 myrecord plate sub 4B67E0 ms is record below zero none age drives slide from -700 3600px per second
    int ghostRecordMs = -1;
    double ghostRecordAge = -1.0;
    float speedKmh = 0.f;
    // 0x4AE230 red gauge band drift charge 0 to 127 below zero hides it
    float driftGauge = -1.f;
    // blue gauge index 1 of same draw only team modes carry it
    float driftGaugeBlue = -1.f;
    // 0x85C spark at O POS node of band while charge runs
    bool driftGaugeSpark = false;
    // 0 none 1 red boost at full band 2 blue seconds since fired lasts 1 7 s
    int gaugeFlash = 0;
    double gaugeFlashAge = 0.0;
    // gauge nifs drawn in world frame so board png and arc stay away
    bool gaugeArt = false;
    int position = 0;
    int racers = 0;
    int lap = 1;
    int totalLaps = 3;
    double raceSeconds = 0.0;
    // best lap of race below zero until a lap closes
    double bestLapSeconds = -1.0;
    // item kinds held minus one means empty slot stock window holds three slots
    int heldItem = -1;
    int heldItem2 = -1;
    int heldItem3 = -1;
    // seconds since last box taken below zero means none banner shows for 0 4 s
    double pickupAge = -1.0;
    int pickupItem = -1;
    // seconds since last hit below zero means none block overlay blinks for 1 5 s
    double hitAge = -1.0;
    int hitItem = -1;
    // sub 4A9DA0 the attack view frame over the top right inset and the name of its car
    bool attackView = false;
    int attackFrame = 0;
    std::string attackName;
    // sub 4AD310 the event view frame at 735 214
    bool eventView = false;
    int eventFrame = 0;
    // sub 4C0740 seconds since the dung hit the own car below zero none
    double dungAge = -1.0;
    // effect 1100 seconds since the flash blinded the own car below zero none
    double flashAge = -1.0;
    // sub 4C9C70 the aim marker of the rocket or the magnet at canvas pixels of its car
    bool reticle = false;
    int reticleKind = 0;
    int reticleSet = 1;
    int reticleFrame = 0;
    bool reticleOnCar = false;
    float reticleX = 0.f;
    float reticleY = 0.f;
    // 0 none 1 to 3 digits 4 is GO seconds since stage began
    int countdownStage = 0;
    // sub 4B2220 Effect startcount nif draws digits in world so sprite stays away
    bool countdownProp = false;
    double countdownAge = 0.0;
    bool waiting = false;
    bool reverse = false;
    // 0 none 1 lap two words 2 final lap words 3 check point pair game stage has none
    int lapFlash = 0;
    double lapFlashAge = 0.0;
    // Panel Stream label slip stream plus n below zero hides it
    float slipStream = -1.f;
    // slope stream plus n reads hud global port lacks so stays hidden
    float slopeStream = -1.f;
    bool finalLap = false;
    bool finished = false;
    int finishRank = -1;
    bool paused = false;
    // board covers middle so side list steps aside
    bool boardOpen = false;
    // running clock in seconds blinking words read it
    double clock = 0.0;
    // 0 item single 1 item team 2 speed single 3 speed team
    uint32_t gameMode = 0;
    std::vector<HudStanding> standings;
    std::vector<HudCar> cars;
    std::vector<HudNameTag> tags;
    const MinimapData* minimap = nullptr;
    // mission world draws map in smaller 240x320 rect icons at scale 0 65
    bool missionMap = false;
    // board rows when result open lands between rank panel and gauge
    const std::vector<HudResultRow>* result = nullptr;
    std::string status;
};

class RaceHud {
public:
    void draw(DrawContext& ctx, AssetStore& assets, const HudState& state, float canvasW, float canvasH);
    // result board of 0x0046 rows over dimmed scene on Result Item or Result Speed art
    void drawResult(DrawContext& ctx, AssetStore& assets, const std::vector<HudResultRow>& rows, uint32_t gameMode,
                    float canvasW, float canvasH);
    // stock quit box from escape key Information art with OK and Cancel halves
    void drawPause(DrawContext& ctx, AssetStore& assets, float canvasW, float canvasH);
    // sub 455430 Information box with any lines OK alone or OK and Cancel halves
    void drawInfoBox(DrawContext& ctx, AssetStore& assets, const std::vector<std::string>& lines, bool cancel);
    // OK rect of one button box
    static Rect infoOkRow();
    // two button rects of quit box index 0 is OK index 1 is Cancel
    static Rect pauseRow(int index);

    // stock item kind names from Icon folder booster big booster spike and so on
    static const char* itemName(int kind);
    // every hud image a race shows a loader thread decodes them so no first use stalls a lap
    static std::vector<std::string> warmList();

private:
    // asset drawn at its own size when pak has it
    bool sprite(DrawContext& ctx, AssetStore& assets, const std::string& path, float x, float y, float scale = 1.f,
                uint32_t colour = kWhite);
    void number(DrawContext& ctx, AssetStore& assets, const std::string& prefix, int value, float x, float y,
                float digitW, float scale, int minDigits);
    // stock time glyphs mm colon ss colon mmm on LapTime num sheet
    void timeDigits(DrawContext& ctx, AssetStore& assets, float x, float y, double seconds);
    // FUN 004B71A0 ghost race record plate slides at plus 300 450 digits at plus 350 550
    void drawGhostRecord(DrawContext& ctx, AssetStore& assets, const HudState& s);
    void drawMinimap(DrawContext& ctx, AssetStore& assets, const HudState& s);
    void drawLapPanel(DrawContext& ctx, AssetStore& assets, const HudState& s);
    void drawRankPanel(DrawContext& ctx, AssetStore& assets, const HudState& s);
    void drawGauge(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH);
    // 0x4AE230 Effect Guage red gauge band posed at fill over 25 4 boost get flash on top
    void drawDriftGauge(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH);
    void drawItemSlot(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW);
    // the dung splats the two preview frames and the aim marker under the panels
    void drawItemViews(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH);
    // effect 1100 the white veil of the flash over the whole hud
    void drawFlash(DrawContext& ctx, const HudState& s, float canvasW, float canvasH);
    void drawMessages(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH);
    // sub 4B2690 modes 3 and 4 lap word pair slides in from both sides holds then leaves
    void drawLapWords(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH);
    // stage 13 key guide from FUN 0041B600 timer from FUN 004B0E40 and instruction strip
    void drawLicence(DrawContext& ctx, AssetStore& assets, const HudState& s);
    // stream label draws at 413 702 number digits at 576 and 596 on 700
    void drawStream(DrawContext& ctx, AssetStore& assets, const HudState& s);
};

}
