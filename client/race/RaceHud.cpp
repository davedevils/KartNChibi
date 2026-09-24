#include "RaceHud.h"

#include "assets/AssetStore.h"
#include "net/Utf.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace KnC::Client {

namespace {

// stock text alpha for hud fonts 6 and 25 is 0xF0 lap words carry 0xDC
constexpr uint32_t kHudInk = rgba(255, 255, 255, 240);
constexpr uint32_t kReverseInk = rgba(255, 255, 255, 208);
constexpr uint32_t kInkDark = rgba(16, 16, 16, 255);

// stock gauge sits at view width minus 264 and view height minus 230
constexpr float kGaugeRight = 264.f;
constexpr float kGaugeBottom = 230.f;
// needle rests at minus 3 14 rad turns 0 0146 rad per kmh capped at 1 24
constexpr float kNeedleRest = -3.14f;
constexpr float kNeedlePerKmh = 0.0146f;
constexpr float kNeedleMax = 1.24f;
// 0x4AE230 band is one nif posed at fill times 0x5A6E90 so 127 is whole five second clip
constexpr float kGaugeFull = 127.f;
// band runs on ring of board png needle pivot is its centre
constexpr float kBandPivotX = 151.f;
constexpr float kBandPivotY = 109.f;
constexpr float kBandInner = 74.f;
constexpr float kBandOuter = 102.f;
constexpr float kBandStart = -3.14f;
constexpr float kBandSweep = 4.38f;
constexpr int kBandSteps = 32;
// 0x5A6E84 red boost get and blue one play for 1 7 s
constexpr float kGaugeFlashSeconds = 1.7f;
// minimap camera rect from stock client view width minus 330 then 96 for 330x440
constexpr float kMapW = 330.f;
constexpr float kMapH = 440.f;
constexpr float kMapTop = 96.f;
// stock frustum fov 1 aspect 0 75 MiniMap icon quads 166x172 units at scale 0 8
constexpr float kIconUnitsX = 166.22616f * 0.8f;
constexpr float kIconUnitsY = 172.50151f * 0.8f;
// stock result rows 8 of 72 px in item modes 16 of 36 px in speed modes
constexpr float kResultX = 153.f;
// sub 4A9DA0 the attack frame at 599 0 and the name at 732 plus 146 on 164
constexpr float kAttackFrameX = 599.f;
constexpr float kAttackFrameY = 0.f;
constexpr float kAttackNameX = 878.f;
constexpr float kAttackNameY = 164.f;
// sub 4AD310 the event frame at 735 214
constexpr float kEventFrameX = 735.f;
constexpr float kEventFrameY = 214.f;
// sub 4C9C70 the marker draws 63 left and 67 up of the aim point its own car mark 17 up
constexpr float kReticleHalf = 63.f;
constexpr float kReticleLift = 67.f;
constexpr float kReticleOwnLift = 17.f;
// sub 4C0740 the four dung splats inside 16000 ms a simplified path of the stock slide
constexpr double kDungSeconds = 16.0;
constexpr double kDungStagger = 0.2;
constexpr double kDungPop = 0.15;
constexpr double kDungHold = 12.0;
constexpr float kDungScale = 0.8f;
constexpr float kDungSlide = 120.f;
constexpr float kDungOffset[4][2] = {{-150.f, -60.f}, {140.f, -90.f}, {-40.f, 80.f}, {170.f, 70.f}};
// sub 43D7E0 mode 3 speed 0x44FA0000 the flash veil lasts 2000 ms
constexpr double kFlashSeconds = 2.0;

const char* const kItemNames[22] = {"booster", "big_booster", "spike", "storm", "thunder", "handle", "turtle", "rabbit",
                                    "shield", "smoke", "rocket", "hive", "angel", "bluerabbit", "ice", "flash",
                                    "magnet", "hammer", "bomb", "dung", "devil", "devilRed"};

std::string clockText(double seconds) {
    if (seconds < 0.0) return "--:--:---";
    const int total = static_cast<int>(seconds * 1000.0);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%03d", std::min(total / 60000, 60), (total / 1000) % 60, total % 1000);
    return buf;
}

// face icon for driver used by minimap and standings
std::string faceIcon(const std::string& driverAsset, bool big) {
    if (driverAsset.empty()) return std::string();
    return "Panel/MiniMap/driver_" + driverAsset + (big ? "_01.dds" : "_02.dds");
}

}

const char* RaceHud::itemName(int kind) {
    return kind >= 0 && kind < 22 ? kItemNames[kind] : "";
}

bool RaceHud::sprite(DrawContext& ctx, AssetStore& assets, const std::string& path, float x, float y, float scale,
                     uint32_t colour) {
    const Texture* tex = assets.texture(path);
    if (!tex || !tex->valid()) return false;
    ctx.batch.draw(tex->handle, x, y, static_cast<float>(tex->width) * scale, static_cast<float>(tex->height) * scale,
                   colour);
    return true;
}

void RaceHud::number(DrawContext& ctx, AssetStore& assets, const std::string& prefix, int value, float x, float y,
                     float digitW, float scale, int minDigits) {
    if (value < 0) value = 0;
    std::string digits = std::to_string(value);
    while (static_cast<int>(digits.size()) < minDigits) digits = "0" + digits;
    float cx = x;
    for (char c : digits) {
        if (!sprite(ctx, assets, prefix + c + ".png", cx, y, scale)) {
            ctx.bold.draw(ctx.batch, std::string(1, c), cx, y, digitW * scale, kWhite);
        }
        cx += digitW * scale;
    }
}

// mm colon ss colon mmm each glyph 29 px apart num 10 is colon
void RaceHud::timeDigits(DrawContext& ctx, AssetStore& assets, float x, float y, double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    int total = static_cast<int>(seconds * 1000.0);
    int minutes = std::min(total / 60000, 60);
    int secs = std::min((total - minutes * 60000) / 1000, 60);
    int ms = std::min(total - 1000 * (secs + 60 * minutes), 999);
    const int glyph[9] = {minutes / 10, minutes % 10, 10, secs / 10, secs % 10, 10, ms / 100, ms % 100 / 10, ms % 10};
    for (int i = 0; i < 9; ++i) {
        char name[48];
        std::snprintf(name, sizeof(name), "Panel/LapTime/num_%02d.png", glyph[i]);
        if (!sprite(ctx, assets, name, x + 29.f * static_cast<float>(i), y)) {
            ctx.bold.draw(ctx.batch, glyph[i] == 10 ? ":" : std::to_string(glyph[i]), x + 29.f * static_cast<float>(i),
                          y + 6.f, 24.f, kWhite);
        }
    }
}

// stock camera looks down from x b y minus a at height c screen up is world minus x
void RaceHud::drawMinimap(DrawContext& ctx, AssetStore& assets, const HudState& s) {
    const MinimapData* m = s.minimap;
    if (!m || !m->valid) return;
    const float distance = m->ini[2] - m->quadZ;
    if (distance <= 1.f) return;
    // mission rect is view width minus 260 then 120 for 240x320 with same frustum
    const float mapW = s.missionMap ? 240.f : kMapW;
    const float mapH = s.missionMap ? 320.f : kMapH;
    const float mapTop = s.missionMap ? 120.f : kMapTop;
    const float scaleV = (mapH / (2.f * 0.54630249f)) / distance;
    const float scaleH = (mapW / (2.f * 0.39359693f)) / distance;
    const float iconScale = s.missionMap ? 0.65f / 0.8f : 1.f;
    const float eyeX = m->ini[1];
    const float eyeY = -m->ini[0];
    const float left = s.missionMap ? 1024.f - 260.f : 1024.f - kMapW;
    const float centreX = left + mapW * 0.5f;
    const float centreY = mapTop + mapH * 0.5f;
    auto project = [&](float wx, float wy, float& sx, float& sy) {
        sx = centreX + (wy - eyeY) * scaleH;
        sy = centreY + (wx - eyeX) * scaleV;
    };
    ctx.batch.setClip(left, mapTop, mapW, mapH);
    const Texture* tex = assets.texture(m->texture);
    if (tex && tex->valid()) {
        float xy[8];
        float uv[8];
        for (int i = 0; i < 4; ++i) {
            project(m->cornerX[i], m->cornerY[i], xy[i * 2], xy[i * 2 + 1]);
            uv[i * 2] = m->cornerU[i];
            uv[i * 2 + 1] = m->cornerV[i];
        }
        ctx.batch.drawQuad(tex->handle, xy, uv);
    }
    // other cars first then local one on top with big face as stock order draws them
    const float iconW = kIconUnitsY * scaleH * iconScale;
    const float iconH = kIconUnitsX * scaleV * iconScale;
    for (int pass = 0; pass < 2; ++pass) {
        for (const HudCar& car : s.cars) {
            if (car.local != (pass == 1)) continue;
            float sx = 0.f, sy = 0.f;
            project(car.x, car.y, sx, sy);
            const Texture* face = assets.texture(faceIcon(car.driverAsset, car.local));
            if (face && face->valid()) ctx.batch.draw(face->handle, sx - iconW * 0.5f, sy - iconH * 0.5f, iconW, iconH);
            else ctx.batch.fill(sx - 4.f, sy - 4.f, 8.f, 8.f, car.local ? rgba(255, 220, 90, 255) : kWhite);
        }
    }
    ctx.batch.clearClip();
}

// LapNum lap slash total at 641 22 then race clock at 739 22 best lap row under it
void RaceHud::drawLapPanel(DrawContext& ctx, AssetStore& assets, const HudState& s) {
    const int total = std::min(std::max(s.totalLaps, 1), 9);
    const int lap = std::min(std::max(s.lap, 1), total);
    char name[48];
    std::snprintf(name, sizeof(name), "Panel/LapTime/LapNum_%02d.png", lap);
    if (!sprite(ctx, assets, name, 641.f, 22.f)) ctx.bold.draw(ctx.batch, std::to_string(lap), 641.f, 28.f, 24.f, kWhite);
    if (!sprite(ctx, assets, "Panel/LapTime/LapNum_10.png", 671.f, 22.f)) ctx.bold.draw(ctx.batch, "/", 671.f, 28.f, 24.f, kWhite);
    std::snprintf(name, sizeof(name), "Panel/LapTime/LapNum_%02d.png", total);
    if (!sprite(ctx, assets, name, 701.f, 22.f)) ctx.bold.draw(ctx.batch, std::to_string(total), 701.f, 28.f, 24.f, kWhite);
    timeDigits(ctx, assets, 739.f, 22.f, s.raceSeconds);
    sprite(ctx, assets, "Panel/LapTime/title_bestTime.png", 617.f, 62.f);
    timeDigits(ctx, assets, 739.f, 62.f, s.bestLapSeconds < 0.0 ? 0.0 : s.bestLapSeconds);
}

// big rank at 8 131 then one row per racer at 14 and 201 plus 40 per row
void RaceHud::drawRankPanel(DrawContext& ctx, AssetStore& assets, const HudState& s) {
    // stock capture shows plate once race runs countdown frames have none
    if (s.racers >= 2 && s.position >= 1 && s.position <= 16 && s.countdownStage == 0 && !s.waiting) {
        char name[64];
        std::snprintf(name, sizeof(name), "Panel/Position/PlayRankingNum_%02d.png", s.position);
        if (!sprite(ctx, assets, name, 8.f, 131.f)) ctx.bold.draw(ctx.batch, std::to_string(s.position), 8.f, 131.f, 48.f, kWhite);
    }
    std::vector<HudStanding> rows = s.standings;
    std::stable_sort(rows.begin(), rows.end(), [](const HudStanding& a, const HudStanding& b) {
        const int pa = a.position < 0 ? 99 : a.position;
        const int pb = b.position < 0 ? 99 : b.position;
        return pa < pb;
    });
    if (s.boardOpen) rows.clear();
    const float x = 14.f;
    float y = 201.f;
    int shown = 0;
    const bool teamMode = s.gameMode == 1 || s.gameMode == 3;
    for (const HudStanding& row : rows) {
        if (shown >= 9) break;
        // dot is red for own side blue for other one in team modes
        const bool otherTeam = teamMode && row.team == 2;
        // capture spots dot at plus 43 7 portrait at plus 6 3 name at plus 68 11
        sprite(ctx, assets, otherTeam ? "Panel/Position/ranking_bar_01.png" : "Panel/Position/ranking_bar_02.png", x + 43.f, y + 7.f);
        if (!row.driverAsset.empty()) {
            if (!sprite(ctx, assets, "Parts/driver_" + row.driverAsset + "_01.png", x + 6.f, y + 3.f, 0.5f))
                sprite(ctx, assets, faceIcon(row.driverAsset, true), x + 4.f, y + 2.f, 0.5f);
        }
        ctx.bold.draw(ctx.batch, row.name, x + 68.f, y + 11.f, 15.f, kHudInk);
        const float nameW = ctx.bold.measure(row.name, 15.f);
        // link icon by ping under 100 ms is four bars over 400 one bar zero means no report
        const int bars = row.pingMs <= 0 ? 0 : row.pingMs <= 100 ? 4 : row.pingMs <= 200 ? 3 : row.pingMs <= 400 ? 2 : 1;
        sprite(ctx, assets, "Icon/link_" + std::to_string(bars) + ".png", x + 68.f + nameW + 8.f, y + 11.f);
        y += 40.f;
        ++shown;
    }
}

// board at right bottom needle under disc then three digits over it
void RaceHud::drawGauge(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH) {
    const float bx = canvasW - kGaugeRight;
    const float by = canvasH - kGaugeBottom;
    // sub 4AE230 race stage draws red gauge nif in place of board gauge scene did it
    const bool artDrawn = s.gaugeArt && s.driftGauge >= 0.f;
    if (!artDrawn && !sprite(ctx, assets, "Panel/Guage/board.png", bx, by))
        ctx.batch.fill(bx, by, 253.f, 212.f, rgba(0, 0, 0, 100));
    const float kmh = std::max(0.f, s.speedKmh);
    float angle = kmh * kNeedlePerKmh + kNeedleRest;
    if (angle > kNeedleMax) angle = kNeedleMax;
    const Texture* arrow = assets.texture("Panel/Guage/speedarrow.png");
    if (arrow && arrow->valid())
        ctx.batch.drawRotated(arrow->handle, bx + 151.f, by + 109.f, 24.f, 89.f, static_cast<float>(arrow->width),
                              static_cast<float>(arrow->height), angle);
    sprite(ctx, assets, "Panel/Guage/speedometer_top.png", bx + 100.f, by + 56.f);
    const int shown = std::min(static_cast<int>(kmh + 0.5f), 999);
    char name[48];
    std::snprintf(name, sizeof(name), "Panel/Guage/speedNum_%d.png", shown / 100);
    sprite(ctx, assets, name, bx + 82.f, by + 82.f);
    std::snprintf(name, sizeof(name), "Panel/Guage/speedNum_%d.png", shown % 100 / 10);
    sprite(ctx, assets, name, bx + 124.f, by + 82.f);
    std::snprintf(name, sizeof(name), "Panel/Guage/speedNum_%d.png", shown % 10);
    sprite(ctx, assets, name, bx + 166.f, by + 82.f);
    if (!artDrawn) drawDriftGauge(ctx, assets, s, canvasW, canvasH);
}

namespace {

// ramp of board redg01 dds green at band start yellow then orange red at full charge
uint32_t bandInk(float f, uint8_t alpha) {
    const float stop[4] = {0.f, 0.45f, 0.75f, 1.f};
    const float r[4] = {60.f, 250.f, 255.f, 255.f};
    const float g[4] = {220.f, 235.f, 150.f, 45.f};
    const float b[4] = {40.f, 40.f, 30.f, 25.f};
    int i = 0;
    while (i < 2 && f > stop[i + 1]) ++i;
    const float span = stop[i + 1] - stop[i];
    const float t = span > 0.f ? (f - stop[i]) / span : 0.f;
    return rgba(static_cast<uint8_t>(r[i] + (r[i + 1] - r[i]) * t), static_cast<uint8_t>(g[i] + (g[i + 1] - g[i]) * t),
                static_cast<uint8_t>(b[i] + (b[i + 1] - b[i]) * t), alpha);
}

}

// 0x4AE230 band of race stage fill eases in 0x4ADC20 resets at 127 with boost get
void RaceHud::drawDriftGauge(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH) {
    if (s.driftGauge < 0.f) return;
    const Texture* white = assets.white();
    if (!white || !white->valid()) return;
    const float px = canvasW - kGaugeRight + kBandPivotX;
    const float py = canvasH - kGaugeBottom + kBandPivotY;
    // boost get plays over whole band for 1 7 s charge starts again under it
    const float flash = s.gaugeFlash > 0 && s.gaugeFlashAge < kGaugeFlashSeconds
                            ? 1.f - static_cast<float>(s.gaugeFlashAge) / kGaugeFlashSeconds
                            : 0.f;
    auto arc = [&](float part, float inner, float outer, bool blue) {
        if (part <= 0.f) return;
        if (part > 1.f) part = 1.f;
        const int steps = static_cast<int>(static_cast<float>(kBandSteps) * part + 0.5f);
        for (int i = 0; i < steps; ++i) {
            const float f0 = static_cast<float>(i) / static_cast<float>(kBandSteps);
            const float f1 = static_cast<float>(i + 1) / static_cast<float>(kBandSteps);
            const float a0 = kBandStart + f0 * kBandSweep;
            const float a1 = kBandStart + f1 * kBandSweep;
            const float d0[2] = {std::sin(a0), -std::cos(a0)};
            const float d1[2] = {std::sin(a1), -std::cos(a1)};
            const float xy[8] = {px + d0[0] * inner, py + d0[1] * inner, px + d0[0] * outer, py + d0[1] * outer,
                                 px + d1[0] * outer, py + d1[1] * outer, px + d1[0] * inner, py + d1[1] * inner};
            const float uv[8] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
            const uint8_t alpha = static_cast<uint8_t>(190.f + 65.f * flash);
            const uint32_t ink = blue ? rgba(70, 150, 255, alpha) : bandInk(f1, alpha);
            ctx.batch.drawQuad(white->handle, xy, uv, ink);
        }
    };
    arc(s.driftGauge / kGaugeFull, kBandInner, kBandOuter, false);
    // second band for team modes ring sector of blue gauge nif was not measured
    if (s.driftGaugeBlue >= 0.f) arc(s.driftGaugeBlue / kGaugeFull, kBandInner - 10.f, kBandInner - 2.f, true);
}

// sub 4B67E0 stage 0xF record plate slides from -1000 at 3600 a second stage 0xB rows fixed at 153 0x4B6D00
void RaceHud::drawGhostRecord(DrawContext& ctx, AssetStore& assets, const HudState& s) {
    if (s.ghostRecordMs < 0 || s.ghostRecordAge < 0.0) return;
    constexpr float kRowStart = -1000.f;
    constexpr float kRowRise = 3600.f;
    constexpr float kPlateX = 300.f;
    constexpr float kPlateY = 450.f;
    constexpr float kDigitsX = 350.f;
    constexpr float kDigitsY = 550.f;
    const float slide = std::min(0.f, kRowStart + static_cast<float>(s.ghostRecordAge) * kRowRise);
    sprite(ctx, assets, "Panel/Result/myrecord.png", kPlateX + slide, kPlateY);
    timeDigits(ctx, assets, kDigitsX + slide, kDigitsY, static_cast<double>(s.ghostRecordMs) / 1000.0);
}

// slot window at 6 3 first item big at 103 27 next two small banner at 200
void RaceHud::drawItemSlot(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW) {
    const float x = 6.f;
    if (s.heldItem3 >= 0) sprite(ctx, assets, std::string("Icon/item_") + itemName(s.heldItem3) + "_s.png", x - 64.f, 53.f);
    if (s.heldItem2 >= 0) sprite(ctx, assets, std::string("Icon/item_") + itemName(s.heldItem2) + "_s.png", x + 13.f, 14.f);
    if (s.heldItem >= 0) {
        const std::string big = std::string("Icon/item_") + itemName(s.heldItem) + "_b.png";
        if (!sprite(ctx, assets, big, x + 97.f, 27.f)) {
            if (!sprite(ctx, assets, "Panel/ItemBlock/" + std::to_string(s.heldItem) + ".png", x + 112.f, 42.f))
                ctx.bold.drawCentered(ctx.batch, std::to_string(s.heldItem), x + 150.f, 60.f, 24.f, kWhite);
        }
    }
    if (!sprite(ctx, assets, "Panel/ItemSlot/slot window.png", x, 3.f)) {
        sprite(ctx, assets, "Panel/ItemSlot/Slot01.png", x + 60.f, 20.f);
    }
    // box just taken shows icon in frame at top middle for four tenths of a second
    if (s.pickupAge >= 0.0 && s.pickupAge < 0.4 && s.pickupItem >= 0) {
        const Texture* frame = assets.texture("Panel/ItemSlot/slot02.png");
        const float frameW = frame && frame->valid() ? static_cast<float>(frame->width) : 102.f;
        const float fx = std::floor((canvasW - frameW) * 0.5f);
        sprite(ctx, assets, "Panel/ItemSlot/slot02.png", fx, 200.f);
        sprite(ctx, assets, std::string("Icon/item_") + itemName(s.pickupItem) + "_b.png", fx + 4.f, 204.f);
    }
    // hit blinks block frame and kind icon right of middle for a second and a half
    if (s.hitAge >= 0.0 && s.hitAge < 1.5) {
        const double phase = s.hitAge * 0.085 * 60.0;
        const bool on = (phase >= 0.0 && phase < 1.0) || (phase >= 1.5 && phase < 2.5) || (phase >= 3.0 && phase < 4.0) ||
                        (phase >= 4.5 && phase < 6.5);
        if (on) {
            sprite(ctx, assets, "Panel/ItemBlock/blocking_00.png", 633.f, 360.f);
            if (s.hitItem >= 0) sprite(ctx, assets, "Panel/ItemBlock/" + std::to_string(s.hitItem) + ".png", 676.f, 398.f);
        }
    }
}

// words at a fifth of height WaitPlayer blinks lap pairs slide in meet hold then leave
void RaceHud::drawMessages(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH) {
    const float y = std::floor(canvasH * 0.2f);
    if (s.waiting && s.countdownStage == 0) {
        const bool on = std::fmod(s.clock, 1.0) < 0.6;
        const Texture* wait = assets.texture("Panel/Message/WaitPlayer.png");
        if (on && wait && wait->valid())
            ctx.batch.draw(wait->handle, std::floor((canvasW - static_cast<float>(wait->width)) * 0.5f), y,
                           static_cast<float>(wait->width), static_cast<float>(wait->height));
        else if (on && !wait)
            ctx.bold.drawCentered(ctx.batch, "Waiting for the players", canvasW * 0.5f, y, 20.f, kWhite);
    }
    // sub 4B23C0 modes 3 and 7 fire only in stages 15 and 17 never in stage 11
    (void)s.lapFlash;
    if (s.reverse && std::fmod(s.clock, 0.8) < 0.4) {
        const Texture* rev = assets.texture("Panel/Message/reverse.png");
        if (rev && rev->valid())
            ctx.batch.draw(rev->handle, std::floor((canvasW - static_cast<float>(rev->width)) * 0.5f),
                           std::floor(canvasH * 0.18f), static_cast<float>(rev->width), static_cast<float>(rev->height),
                           kReverseInk);
    }
}

// label at 413 702 sub 4B75F0 puts tens at 576 and units at 596 on 700
void RaceHud::drawStream(DrawContext& ctx, AssetStore& assets, const HudState& s) {
    const bool slope = s.slipStream <= 0.f && s.slopeStream > 0.f;
    const float value = slope ? s.slopeStream : s.slipStream;
    if (value <= 0.f) return;
    sprite(ctx, assets, slope ? "Panel/Stream/slope_stream.png" : "Panel/Stream/slip_stream.png", 413.f, 702.f);
    const int n = std::min(std::max(static_cast<int>(value), 0), 99);
    char name[64];
    if (n / 10 > 0) {
        std::snprintf(name, sizeof(name), "Panel/Stream/num_%d.png", n / 10);
        sprite(ctx, assets, name, 576.f, 700.f);
    }
    std::snprintf(name, sizeof(name), "Panel/Stream/num_%d.png", n % 10);
    sprite(ctx, assets, name, 596.f, 700.f);
}

// pair meets 4 px off centre at a fifth of height holds 2000 or 600 ms
void RaceHud::drawLapWords(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH) {
    if (s.lapFlash != 2 && s.lapFlash != 3) return;
    const bool check = s.lapFlash == 3;
    const Texture* left = assets.texture(check ? "Panel/Message/Check.png" : "Panel/Message/final.png");
    const Texture* right = assets.texture(check ? "Panel/Message/Point.png" : "Panel/Message/lap.png");
    if (!left || !left->valid() || !right || !right->valid()) return;
    const float lw = static_cast<float>(left->width);
    const float rw = static_cast<float>(right->width);
    const float y = std::floor(canvasH * 0.2f);
    // 0x5A32A0 slide is 20 px per 60 Hz frame rest is hold of mode
    const float slide = 1200.f;
    const float hold = check ? 0.6f : 2.0f;
    const float inSeconds = (canvasW * 0.5f + lw) / slide;
    const float age = static_cast<float>(s.lapFlashAge);
    float offset = 0.f;
    if (age < inSeconds) offset = (inSeconds - age) * slide;
    else if (age < inSeconds + hold) offset = 0.f;
    else if (age < inSeconds * 2.f + hold) offset = -(age - inSeconds - hold) * slide;
    else return;
    const float lx = std::floor(canvasW * 0.5f - 4.f - lw - offset);
    const float rx = std::floor(canvasW * 0.5f + 4.f + offset);
    ctx.batch.draw(left->handle, lx, y, lw, static_cast<float>(left->height), rgba(255, 255, 255, 220));
    ctx.batch.draw(right->handle, rx, y, rw, static_cast<float>(right->height), rgba(255, 255, 255, 220));
}

// FUN 0041B600 arrows 620 135 695 175 635 175 575 175 shift 285 135 ctrl 260 170 c art held
void RaceHud::drawLicence(DrawContext& ctx, AssetStore& assets, const HudState& s) {
    const char* const names[6] = {"up", "right", "down", "left", "shift", "ctrl"};
    const float at[6][2] = {{620.f, 135.f}, {695.f, 175.f}, {635.f, 175.f}, {575.f, 175.f}, {285.f, 135.f}, {260.f, 170.f}};
    for (int i = 0; i < 6; ++i) {
        const std::string art = std::string("Arrow/key_") + names[i] + (s.licenceKeys[i] ? "_c.png" : "_n.png");
        sprite(ctx, assets, art, at[i][0], at[i][1]);
    }
    if (!s.licenceLine.empty()) {
        const Texture* line = assets.texture(s.licenceLine);
        if (line && line->valid())
            ctx.batch.draw(line->handle, std::floor(529.f - static_cast<float>(line->width) * 0.5f),
                           std::floor(29.f - static_cast<float>(line->height) * 0.5f), static_cast<float>(line->width),
                           static_cast<float>(line->height));
    }
    // FUN 004B0E40 400 50 nine LapTime glyphs for time left
    if (s.licenceTimerMs >= 0.0) timeDigits(ctx, assets, 400.f, 50.f, s.licenceTimerMs / 1000.0);
}

// sub 4C0740 four splats land one after the other hold then slide down and fade inside 16000 ms
void RaceHud::drawItemViews(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH) {
    if (s.dungAge >= 0.0 && s.dungAge < kDungSeconds) {
        for (int i = 0; i < 4; ++i) {
            const double age = s.dungAge - static_cast<double>(i) * kDungStagger;
            if (age < 0.0) continue;
            char name[64];
            std::snprintf(name, sizeof(name), "Data/Public/Item/Dung/item_ddon_%02d.png", i);
            const Texture* tex = assets.texture(name);
            if (!tex || !tex->valid()) continue;
            const float pop = static_cast<float>(std::min(1.0, age / kDungPop));
            const float scale = kDungScale * (0.7f + 0.3f * pop);
            const float fade = static_cast<float>(std::max(0.0, (s.dungAge - kDungHold) / (kDungSeconds - kDungHold)));
            const float w = static_cast<float>(tex->width) * scale;
            const float h = static_cast<float>(tex->height) * scale;
            const float x = canvasW * 0.5f + kDungOffset[i][0] - w * 0.5f;
            const float y = canvasH * 0.5f + kDungOffset[i][1] - h * 0.5f + fade * kDungSlide;
            ctx.batch.draw(tex->handle, std::floor(x), std::floor(y), w, h,
                           rgba(255, 255, 255, static_cast<uint8_t>((1.f - fade) * 255.f)));
        }
    }
    // sub 4A9DA0 the frame pngs swap every second frame the name sits under the inset centre
    if (s.attackView) {
        char name[64];
        std::snprintf(name, sizeof(name), "Panel/AttackView/AttackView_%02d.png", s.attackFrame + 1);
        sprite(ctx, assets, name, kAttackFrameX, kAttackFrameY);
        if (!s.attackName.empty()) {
            const float w = ctx.bold.measure(s.attackName, 13.f);
            const float x = std::floor(kAttackNameX - w * 0.5f);
            ctx.bold.draw(ctx.batch, s.attackName, x + 1.f, kAttackNameY + 1.f, 13.f, rgba(0, 0, 0, 200));
            ctx.bold.draw(ctx.batch, s.attackName, x, kAttackNameY, 13.f, kWhite);
        }
    }
    if (s.eventView) {
        char name[64];
        std::snprintf(name, sizeof(name), "Panel/EventView2/EventView_%02d.png", s.eventFrame + 1);
        sprite(ctx, assets, name, kEventFrameX, kEventFrameY);
    }
    // sub 4C9C70 the marker at alpha 0xA0 its own car mark sits 50 lower than the aim of the shooter
    if (s.reticle) {
        char name[80];
        std::snprintf(name, sizeof(name), "Data/Public/Item/%s/target%02d_%02d.PNG",
                      s.reticleKind == 16 ? "Magnet" : "Rocket", s.reticleSet, s.reticleFrame);
        const float y = s.reticleOnCar ? s.reticleY - kReticleOwnLift : s.reticleY - kReticleLift;
        sprite(ctx, assets, name, std::floor(s.reticleX - kReticleHalf), std::floor(y), 1.f, rgba(255, 255, 255, 160));
    }
}

// sub 43D7E0 mode 3 the flash veil fades out over 2000 ms
void RaceHud::drawFlash(DrawContext& ctx, const HudState& s, float canvasW, float canvasH) {
    if (s.flashAge < 0.0 || s.flashAge >= kFlashSeconds) return;
    const float alpha = 1.f - static_cast<float>(s.flashAge / kFlashSeconds);
    ctx.batch.fill(0.f, 0.f, canvasW, canvasH, rgba(255, 255, 255, static_cast<uint8_t>(alpha * 255.f)));
}

void RaceHud::draw(DrawContext& ctx, AssetStore& assets, const HudState& s, float canvasW, float canvasH) {
    // stock intro pan shows only wait words panels come with chase camera
    const bool panelsHidden = s.waiting && s.countdownStage == 0;
    // sub 4A8D80 the preview views draw before the panels of the hud
    if (!panelsHidden) drawItemViews(ctx, assets, s, canvasW, canvasH);
    if (!panelsHidden && s.kind == HudKind::Licence) {
        // panel manager draws minimap gauge and item slot in stage 13 rest is ours
        drawMinimap(ctx, assets, s);
        drawGauge(ctx, assets, s, canvasW, canvasH);
        drawItemSlot(ctx, assets, s, canvasW);
        drawLicence(ctx, assets, s);
    } else if (!panelsHidden && s.kind == HudKind::Mission) {
        // FUN 004A8D80 stage 0x19 minimap item slot then FUN 00439DB0 draws time left
        drawMinimap(ctx, assets, s);
        drawGauge(ctx, assets, s, canvasW, canvasH);
        drawItemSlot(ctx, assets, s, canvasW);
        if (s.licenceTimerMs >= 0.0) timeDigits(ctx, assets, 400.f, 50.f, s.licenceTimerMs / 1000.0);
    } else if (!panelsHidden && s.kind == HudKind::Ghost) {
        // stage 15 minimap lap panel gauge and item slot no rank plate no standings
        drawMinimap(ctx, assets, s);
        drawLapPanel(ctx, assets, s);
        drawGauge(ctx, assets, s, canvasW, canvasH);
        drawItemSlot(ctx, assets, s, canvasW);
        drawLapWords(ctx, assets, s, canvasW, canvasH);
        drawGhostRecord(ctx, assets, s);
    } else if (!panelsHidden) {
        drawMinimap(ctx, assets, s);
        drawLapPanel(ctx, assets, s);
        drawRankPanel(ctx, assets, s);
        if (s.result) drawResult(ctx, assets, *s.result, s.gameMode, canvasW, canvasH);
        drawGauge(ctx, assets, s, canvasW, canvasH);
        drawItemSlot(ctx, assets, s, canvasW);
        drawStream(ctx, assets, s);
    }
    // stock capture floats name of every other racer over its kart in small dark plate
    if (!panelsHidden) {
        for (const HudNameTag& tag : s.tags) {
            const float w = ctx.bold.measure(tag.name, 11.f) + 8.f;
            const float x = std::floor(tag.x - w * 0.5f);
            const float y = std::floor(tag.y - 8.f);
            ctx.batch.fill(x, y, w, 16.f, rgba(30, 30, 30, 150));
            ctx.bold.draw(ctx.batch, tag.name, x + 4.f, y + 2.f, 11.f, kWhite);
        }
    }
    drawMessages(ctx, assets, s, canvasW, canvasH);

    // stock startcount effect grows at centre of 512 272 GO swells and fades
    if (s.countdownStage >= 1 && s.countdownStage <= 4 && !s.countdownProp) {
        char name[64];
        std::snprintf(name, sizeof(name), "Panel/Message/start_%02d.png", s.countdownStage);
        const Texture* tex = assets.texture(name);
        if (tex && tex->valid()) {
            const float t = std::min(std::max(static_cast<float>(s.countdownAge), 0.f), 1.f);
            const bool go = s.countdownStage == 4;
            const float scale = go ? 1.f + 0.4f * t : 0.7f + 0.35f * t;
            const float alpha = go ? (t < 0.7f ? 1.f : 1.f - (t - 0.7f) / 0.3f) : 1.f;
            const float w = static_cast<float>(tex->width) * scale;
            const float h = static_cast<float>(tex->height) * scale;
            const uint32_t ink = rgba(255, 255, 255, static_cast<uint8_t>(alpha * 255.f));
            ctx.batch.draw(tex->handle, std::floor(canvasW * 0.5f - w * 0.5f), std::floor(272.f - h * 0.5f), w, h, ink);
        } else {
            const char* text = s.countdownStage == 4 ? "GO" : s.countdownStage == 3 ? "3" : s.countdownStage == 2 ? "2" : "1";
            ctx.bold.drawCentered(ctx.batch, text, canvasW * 0.5f, canvasH * 0.5f - 40.f, 80.f, rgba(255, 220, 90, 255));
        }
    }
    // team modes name side that won single modes wait for board
    if (s.finished && s.finishRank >= 0 && (s.gameMode == 1 || s.gameMode == 3)) {
        const char* art = s.finishRank == 0 ? "Panel/Result/Result_Winner.png" : "Panel/Result/Result_Loser.png";
        const Texture* tex = assets.texture(art);
        if (tex && tex->valid())
            ctx.batch.draw(tex->handle, std::floor((canvasW - static_cast<float>(tex->width)) * 0.5f), 300.f,
                           static_cast<float>(tex->width), static_cast<float>(tex->height));
    }
    drawFlash(ctx, s, canvasW, canvasH);
}

void RaceHud::drawResult(DrawContext& ctx, AssetStore& assets, const std::vector<HudResultRow>& board, uint32_t gameMode,
                         float canvasW, float canvasH) {
    ctx.batch.fill(0.f, 0.f, canvasW, canvasH, rgba(0, 0, 0, 96));
    const bool itemMode = gameMode == 0 || gameMode == 1;
    const char* art = itemMode ? "Panel/Result/Result_Item.png" : "Panel/Result/Result_Speed.png";
    if (!sprite(ctx, assets, art, 37.f, 58.f)) {
        ctx.batch.fill(37.f, 58.f, 942.f, 659.f, rgba(28, 30, 48, 235));
        ctx.bold.drawCentered(ctx.batch, "Race result", 37.f + 471.f, 66.f, 18.f, kWhite);
    }
    std::vector<HudResultRow> rows = board;
    std::sort(rows.begin(), rows.end(), [](const HudResultRow& a, const HudResultRow& b) { return a.rank < b.rank; });
    const int maxRows = itemMode ? 8 : 16;
    const float rowStep = itemMode ? 72.f : 36.f;
    float y = itemMode ? 158.f : 139.f;
    int shown = 0;
    for (const HudResultRow& r : rows) {
        if (shown >= maxRows) break;
        const std::string face = faceIcon(r.driverAsset, true);
        if (!face.empty()) sprite(ctx, assets, face, kResultX, y - 12.f, 0.5f);
        ctx.bold.draw(ctx.batch, r.name, kResultX + 90.f, y + 8.f, 17.f, kHudInk);
        ctx.bold.draw(ctx.batch, clockText(r.timeMs > 0 ? r.timeMs / 1000.0 : -1.0), kResultX + 366.f, y + 8.f, 17.f, kHudInk);
        std::string exp = std::to_string(r.exp);
        if (r.expBonus) exp += " + " + std::to_string(r.expBonus);
        std::string gold = std::to_string(r.gold);
        if (r.goldBonus) gold += " + " + std::to_string(r.goldBonus);
        ctx.bold.draw(ctx.batch, exp, kResultX + 560.f - ctx.bold.measure(exp, 17.f), y + 8.f, 17.f, kHudInk);
        ctx.bold.draw(ctx.batch, gold, kResultX + 720.f - ctx.bold.measure(gold, 17.f), y + 8.f, 17.f, kHudInk);
        y += rowStep;
        ++shown;
    }
    if (rows.empty()) ctx.bold.drawCentered(ctx.batch, "waiting for the 0x0046 board", canvasW * 0.5f, 400.f, 15.f, kWhite);
}

// stock quit box at 339 279 OK half at plus 80 126 Cancel at plus 174 126
Rect RaceHud::pauseRow(int index) {
    const float bx = 339.f;
    const float by = 279.f;
    if (index == 0) return {bx + 80.f, by + 126.f, 94.f, 29.f};
    return {bx + 174.f, by + 126.f, 94.f, 29.f};
}

Rect RaceHud::infoOkRow() {
    return {339.f + 126.f, 279.f + 126.f, 94.f, 29.f};
}

// lines centred on box middle 20 px apart OK whole at plus 126 or two halves
void RaceHud::drawInfoBox(DrawContext& ctx, AssetStore& assets, const std::vector<std::string>& lines, bool cancel) {
    const float bx = 339.f;
    const float by = 279.f;
    if (!sprite(ctx, assets, "Popup/Message/Popup_Information.png", bx, by)) ctx.batch.fill(bx, by, 347.f, 170.f, rgba(230, 230, 230, 240));
    float y = by + 72.f - 10.f * static_cast<float>(lines.size());
    for (const std::string& line : lines) {
        ctx.bold.drawCentered(ctx.batch, line, bx + 173.f, y, 15.f, kInkDark);
        y += 20.f;
    }
    if (cancel) {
        const Rect ok = pauseRow(0);
        const Rect no = pauseRow(1);
        if (!sprite(ctx, assets, "Buttons/Common_OK_half_00.png", ok.x, ok.y))
            ctx.bold.drawCentered(ctx.batch, "OK", ok.x + ok.w * 0.5f, ok.y + 6.f, 15.f, kInkDark);
        if (!sprite(ctx, assets, "Buttons/Common_Cancel_half_00.png", no.x, no.y))
            ctx.bold.drawCentered(ctx.batch, "Cancel", no.x + no.w * 0.5f, no.y + 6.f, 15.f, kInkDark);
        return;
    }
    const Rect ok = infoOkRow();
    if (!sprite(ctx, assets, "Buttons/Common_OK_half_00.png", ok.x, ok.y))
        ctx.bold.drawCentered(ctx.batch, "OK", ok.x + ok.w * 0.5f, ok.y + 6.f, 15.f, kInkDark);
}

// stock escape asks are you sure in Information box no menu opens
void RaceHud::drawPause(DrawContext& ctx, AssetStore& assets, float canvasW, float canvasH) {
    (void)canvasW;
    (void)canvasH;
    const float bx = 339.f;
    const float by = 279.f;
    if (!sprite(ctx, assets, "Popup/Message/Popup_Information.png", bx, by)) ctx.batch.fill(bx, by, 347.f, 170.f, rgba(230, 230, 230, 240));
    ctx.bold.drawCentered(ctx.batch, "Are you sure you want to quit?", bx + 173.f, by + 62.f, 15.f, kInkDark);
    ctx.bold.drawCentered(ctx.batch, "You may lose some Points and Exp", bx + 173.f, by + 82.f, 15.f, kInkDark);
    const Rect ok = pauseRow(0);
    const Rect cancel = pauseRow(1);
    if (!sprite(ctx, assets, "Buttons/Common_OK_half_00.png", ok.x, ok.y))
        ctx.bold.drawCentered(ctx.batch, "OK", ok.x + ok.w * 0.5f, ok.y + 6.f, 15.f, kInkDark);
    if (!sprite(ctx, assets, "Buttons/Common_Cancel_half_00.png", cancel.x, cancel.y))
        ctx.bold.drawCentered(ctx.batch, "Cancel", cancel.x + cancel.w * 0.5f, cancel.y + 6.f, 15.f, kInkDark);
}

// a first use was a file read a png decode and an upload on the frame the image showed
std::vector<std::string> RaceHud::warmList() {
    std::vector<std::string> list;
    char name[64];
    for (int i = 0; i <= 10; ++i) {
        std::snprintf(name, sizeof(name), "Panel/LapTime/num_%02d.png", i);
        list.push_back(name);
        if (i == 0) continue;
        std::snprintf(name, sizeof(name), "Panel/LapTime/LapNum_%02d.png", i);
        list.push_back(name);
    }
    for (int i = 0; i <= 9; ++i) {
        std::snprintf(name, sizeof(name), "Panel/Guage/speedNum_%d.png", i);
        list.push_back(name);
        std::snprintf(name, sizeof(name), "Panel/Stream/num_%d.png", i);
        list.push_back(name);
    }
    for (int i = 1; i <= 8; ++i) {
        std::snprintf(name, sizeof(name), "Panel/Position/PlayRankingNum_%02d.png", i);
        list.push_back(name);
    }
    for (int i = 1; i <= 4; ++i) {
        std::snprintf(name, sizeof(name), "Panel/Message/start_%02d.png", i);
        list.push_back(name);
    }
    for (int i = 0; i <= 4; ++i) list.push_back("Icon/link_" + std::to_string(i) + ".png");
    for (const char* item : kItemNames) {
        list.push_back(std::string("Icon/item_") + item + "_b.png");
        list.push_back(std::string("Icon/item_") + item + "_s.png");
    }
    for (const char* art : {"Panel/LapTime/title_bestTime.png", "Panel/Position/ranking_bar_01.png",
                            "Panel/Position/ranking_bar_02.png", "Panel/Guage/board.png", "Panel/Guage/speedarrow.png",
                            "Panel/Guage/speedometer_top.png", "Panel/ItemSlot/slot window.png",
                            "Panel/ItemSlot/Slot01.png", "Panel/ItemSlot/slot02.png", "Panel/Message/WaitPlayer.png",
                            "Panel/Message/reverse.png", "Panel/Message/Check.png", "Panel/Message/Point.png",
                            "Panel/Message/final.png", "Panel/Message/lap.png", "Panel/ItemBlock/blocking_00.png",
                            "Panel/Stream/slip_stream.png", "Panel/Stream/slope_stream.png"})
        list.push_back(art);
    return list;
}

}
