#include "MenuFrame.h"

#include "WidgetScreen.h"
#include "app/App.h"
#include "assets/AssetStore.h"
#include "net/Utf.h"
#include "screens/GachaPopup.h"
#include "screens/GhostModeScreen.h"
#include "screens/LicenceScreen.h"
#include "screens/MenuPopup.h"

#include <algorithm>
#include <cstdio>

namespace KnC::Client {

namespace {

// one button of the stock frame class 0x42CF50 live buttons wear the blue art as the stock lobby shows them
struct FrameButton {
    const char* art;
    float x;
    float y;
    const char* action;
    bool live;
};

const FrameButton kFullTop[] = {
    {"Menu/channel_lobby_", 25.f, 4.f, "channel", true},
    {"Menu/Common_Top_Lobby_", 205.f, 4.f, "lobby", true},
    {"Menu/Common_Top_Ghost_", 327.f, 4.f, "ghost", true},
    {"Menu/Common_Top_Quest_", 449.f, 4.f, "quest", false},
    {"Menu/Common_Top_Tutorial_", 571.f, 4.f, "tutorial", true},
    {"Menu/Common_Top_Mission_", 693.f, 4.f, "missions", true},
};

const FrameButton kFullBottom[] = {
    {"Menu/Common_Bottom_Shop_", 68.f, 731.f, "shop", true},
    {"Menu/Common_Bottom_Gacha_", 186.f, 731.f, "gacha", true},
    {"Menu/Common_Bottom_Garage_", 370.f, 731.f, "garage", true},
    {"Menu/Common_Bottom_CarCraft_", 488.f, 731.f, "carcraft", true},
    {"Menu/Common_Bottom_RoomCraft_", 606.f, 731.f, "roomcraft", true},
    {"Menu/Common_Option_", 928.f, 734.f, "option", true},
    {"Menu/Common_Community_", 958.f, 734.f, "messenger", true},
    {"Menu/Common_Quit_", 988.f, 734.f, "quit", true},
};

const FrameButton kRoomTop[] = {
    {"Menu/Common_Top_Lobby_", 17.f, 4.f, "lobby", true},
    {"Menu/Common_Option_", 959.f, 6.f, "option", true},
    {"Menu/Common_Community_", 990.f, 6.f, "messenger", true},
};

void addButton(WidgetScreen& screen, AssetStore& assets, const FrameButton& b, const std::string& current) {
    const std::string art = b.art;
    if (!b.live) {
        auto img = std::make_unique<ImageWidget>();
        img->type = ElementType::Image;
        img->id = std::string("frame_") + b.action;
        img->texture = assets.texture(art + "03.png");
        if (!img->texture) img->texture = assets.texture(art + "00.png");
        img->rect = {b.x, b.y, img->texture ? static_cast<float>(img->texture->width) : 100.f,
                     img->texture ? static_cast<float>(img->texture->height) : 31.f};
        img->zIndex = 18;
        screen.add(std::move(img));
        return;
    }
    auto btn = std::make_unique<ButtonWidget>();
    btn->type = ElementType::Button;
    btn->id = std::string("frame_") + b.action;
    btn->action = b.action;
    const bool here = current == b.action;
    btn->normal = assets.texture(art + (here ? "02.png" : "00.png"));
    btn->hover = assets.texture(art + (here ? "02.png" : "01.png"));
    btn->pressed = assets.texture(art + "02.png");
    const Texture* size = btn->normal ? btn->normal : btn->hover;
    btn->rect = {b.x, b.y, size ? static_cast<float>(size->width) : 100.f, size ? static_cast<float>(size->height) : 31.f};
    btn->zIndex = 18;
    screen.add(std::move(btn));
}

}

void addMenuFrame(WidgetScreen& screen, AssetStore& assets, FrameMode mode, const std::string& current) {
    if (mode == FrameMode::Full) {
        auto wall = std::make_unique<ImageWidget>();
        wall->type = ElementType::Image;
        wall->id = "frame_wallpaper";
        wall->texture = assets.texture("Wallpaper/BackImage_00.png");
        wall->rect = {0.f, 0.f, 1024.f, 768.f};
        wall->zIndex = -20;
        screen.add(std::move(wall));
        auto back = std::make_unique<ImageWidget>();
        back->type = ElementType::Image;
        back->id = "frame_back";
        back->texture = assets.texture("Menu/Common_back.png");
        back->rect = {0.f, 0.f, 1024.f, 768.f};
        back->zIndex = -19;
        screen.add(std::move(back));
        for (const FrameButton& b : kFullTop) addButton(screen, assets, b, current);
        for (const FrameButton& b : kFullBottom) addButton(screen, assets, b, current);
    } else {
        for (const FrameButton& b : kRoomTop) addButton(screen, assets, b, current);
    }
}

bool frameAction(App& app, const std::string& action) {
    if (action == "gacha") { app.pushScreen(std::make_unique<GachaPopup>(app)); return true; }
    // the gear is case 0xb of the frame router sub 42BCE0 the Menu box of sub 463BA0
    if (action == "option") { app.pushScreen(std::make_unique<MenuPopup>(app)); return true; }
    // the Channel button is case 0xe the 0x0019 return to the channel list
    if (action == "channel") { app.session().returnToChannels(); return true; }
    if (action == "tutorial") { app.session().openLicense(); return true; }
    // sub 42BCE0 cases 1 and 2 the ghost and mission menus past level 9 or with a licence grade
    if (action == "ghost") {
        if (!GhostModeScreen::open(app)) std::printf("[frame] ghost needs level 10 or a licence grade\n");
        return true;
    }
    if (action == "missions") {
        if (LicenceScreen::stageGateOpen(app)) app.session().openMissionMenu();
        else std::printf("[frame] missions need level 10 or a licence grade\n");
        return true;
    }
    // the two craft stages have no wire on our server the screens open on their own
    if (action == "carcraft") { app.go("carcraft"); return true; }
    if (action == "roomcraft") { app.go("roomcraft"); return true; }
    return false;
}

// four slices of the image leave the hole open the hole is clipped to the image
void drawTextureWithHole(SpriteBatch& batch, const Texture* tex, float x, float y, const Rect& hole) {
    if (!tex || !tex->valid()) return;
    const float w = static_cast<float>(tex->width);
    const float h = static_cast<float>(tex->height);
    const float hl = std::min(std::max(hole.x - x, 0.f), w);
    const float ht = std::min(std::max(hole.y - y, 0.f), h);
    const float hr = std::min(std::max(hole.x + hole.w - x, 0.f), w);
    const float hb = std::min(std::max(hole.y + hole.h - y, 0.f), h);
    if (hole.w <= 0.f || hole.h <= 0.f || hr <= hl || hb <= ht) {
        batch.draw(tex->handle, x, y, w, h);
        return;
    }
    const float u0 = hl / w, u1 = hr / w;
    const float v0 = ht / h, v1 = hb / h;
    if (ht > 0.f) batch.draw(tex->handle, x, y, w, ht, kWhite, 0.f, 0.f, 1.f, v0);
    if (hb < h) batch.draw(tex->handle, x, y + hb, w, h - hb, kWhite, 0.f, v1, 1.f, 1.f);
    if (hl > 0.f) batch.draw(tex->handle, x, y + ht, hl, hb - ht, kWhite, 0.f, v0, u0, v1);
    if (hr < w) batch.draw(tex->handle, x + hr, y + ht, w - hr, hb - ht, kWhite, u1, v0, 1.f, v1);
}

void drawFrameBack(SpriteBatch& batch, AssetStore& assets, const Rect& hole) {
    drawTextureWithHole(batch, assets.texture("Wallpaper/BackImage_00.png"), 0.f, 0.f, hole);
    drawTextureWithHole(batch, assets.texture("Menu/Common_back.png"), 0.f, 0.f, hole);
}

void drawAligned(DrawContext& ctx, const FontAtlas& font, const std::string& text, float x, float y, float px,
                 uint32_t colour, Align align) {
    float at = x;
    if (align == Align::Centre) at = x - font.measure(text, px) * 0.5f;
    else if (align == Align::Right) at = x - font.measure(text, px);
    font.draw(ctx.batch, text, at, y, px, colour);
}

// sub 429990 level icon 57 593 name centred 225 the bar from 108 the wallets on 666
void drawCharInfo(DrawContext& ctx, AssetStore& assets, const Profile& p) {
    char name[64];
    std::snprintf(name, sizeof(name), "Icon/lv_icon_s_%03d.png", std::min(static_cast<int>(p.level) + 1, 50));
    const Texture* badge = assets.texture(name);
    if (badge && badge->valid()) ctx.batch.draw(badge->handle, 57.f, 593.f, static_cast<float>(badge->width), static_cast<float>(badge->height));
    drawAligned(ctx, ctx.bold, u16ToUtf8(p.nickname), 225.f, 604.f, 15.f, kInkBlack, Align::Centre);
    const float span = static_cast<float>(p.expNext) - static_cast<float>(p.expFloor);
    float ratio = span > 0.f ? (static_cast<float>(p.exp) - static_cast<float>(p.expFloor)) / span : 0.f;
    ratio = std::min(std::max(ratio, 0.f), 1.f);
    const Texture* bar = assets.texture("CharInfo/Common_Info_Bar.png");
    const float width = std::floor(256.f * ratio);
    if (width > 0.f) {
        if (bar && bar->valid()) ctx.batch.draw(bar->handle, 108.f, 632.f, width, static_cast<float>(bar->height));
        else ctx.batch.fill(108.f, 632.f, width, 16.f, rgba(80, 180, 255, 255));
    }
    drawAligned(ctx, ctx.bold, "EXP : " + std::to_string(p.exp) + " / " + std::to_string(p.expNext), 232.f, 633.f, 15.f, kInkBlack, Align::Centre);
    // the exe strings read As and Go the running client prints Astro and Gold the capture wins
    ctx.bold.draw(ctx.batch, "Astro", 65.f, 666.f, 15.f, kInkBlack);
    drawAligned(ctx, ctx.bold, std::to_string(p.astro), 219.f, 666.f, 15.f, kInkBlack, Align::Right);
    ctx.bold.draw(ctx.batch, "Gold", 237.f, 666.f, 15.f, kInkBlack);
    drawAligned(ctx, ctx.bold, std::to_string(p.gold), 390.f, 666.f, 15.f, kInkBlack, Align::Right);
}

}
