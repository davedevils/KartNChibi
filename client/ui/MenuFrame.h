// the stock menu frame the wallpaper the common back the top and bottom buttons and the char info column
#pragma once

#include "Widgets.h"
#include "net/Session.h"

#include <array>
#include <string>

namespace KnC::Client {

class AssetStore;
class WidgetScreen;

// the full frame of the lobby garage and shop or the short top row of the waiting room
enum class FrameMode { Full, Room };

// adds the frame widgets actions channel lobby missions shop garage messenger quit current tab wears pressed art rest wear grey
void addMenuFrame(WidgetScreen& screen, AssetStore& assets, FrameMode mode, const std::string& current);

// the level badge the name the exp bar and the two wallets at the stock spots of the left column
void drawCharInfo(DrawContext& ctx, AssetStore& assets, const Profile& profile);

class App;
// the frame buttons gacha opens its box ghost and missions gate on level 10 or licence grade sub 42BCE0
bool frameAction(App& app, const std::string& action);

// the wallpaper and the common back drawn around a hole so a 3D preview under the sprites shows through
void drawFrameBack(SpriteBatch& batch, AssetStore& assets, const Rect& hole);
// one texture at x y drawn in four slices around the hole an empty hole draws it whole
void drawTextureWithHole(SpriteBatch& batch, const Texture* tex, float x, float y, const Rect& hole);

// the stock text alignment left centre right of one line at x
enum class Align { Left, Centre, Right };
void drawAligned(DrawContext& ctx, const FontAtlas& font, const std::string& text, float x, float y, float px,
                 uint32_t colour, Align align);

// the stock font table colours black white and the grey of the info texts
constexpr uint32_t kInkBlack = rgba(0, 0, 0, 255);
constexpr uint32_t kInkWhite = rgba(255, 255, 255, 255);
constexpr uint32_t kInkGrey = rgba(96, 95, 96, 255);
constexpr uint32_t kInkDark = rgba(16, 16, 16, 255);

}
