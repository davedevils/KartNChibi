// what the garage and the shop share catalogue names prices icons and word wrap
#pragma once

#include "net/Catalog.h"
#include "ui/FontAtlas.h"

#include <string>
#include <vector>

namespace KnC::Client {

class App;
class AssetStore;

// the 0x00B7 categories 0 character 1 kart 2 consumable 3 part 4 pet 5 room craft 6 car craft
enum class BuyCategory : uint32_t {
    Character = 0,
    Kart = 1,
    Consumable = 2,
    Part = 3,
    Pet = 4,
    RoomCraft = 5,
    CarCraft = 6,
};

// one price option joined with its 0x00C6 row
struct PriceQuote {
    uint32_t priceKey = 0;
    uint32_t unitType = 0;
    uint32_t unitAmount = 0;
    uint32_t priceBase = 0;
    uint32_t priceSale = 0;
    bool resolved = false;
    // what is charged the sale value wins when it is above zero
    uint32_t charged() const { return priceSale > 0 ? priceSale : priceBase; }
    std::string unitText() const;
    std::string priceText() const;
};

// joins the inline price options of a def with the price table
std::vector<PriceQuote> quotesOf(const Catalog& catalog, const std::vector<PriceOption>& options);

// the icon of a catalogue row under Image Parts 01 is 64 px 02 is 128 px
std::string kartIcon(const KartRow& row, bool big = false);
std::string driverIcon(const DriverRow& row, bool big = false);
std::string itemIcon(const ItemRow& row, bool big = false);
std::string partIcon(const PartRow& row, bool big = false);
std::string petIcon(const PetRow& row, bool big = false);

// the first path that decodes else the second our server can swap the icon and the name of 0x00C1
std::string pickIcon(AssetStore& assets, const std::string& first, const std::string& second);

// breaks a def trans line into drawn lines the two character backslash n counts as a break
std::vector<std::string> wrapText(const FontAtlas& font, const std::string& utf8, float px, float maxWidth);

// equip slot tab filter 0x418C80
bool partInTab(const PartRow& row, int category, int sub);

// the restrict rule of part restrict check 0x415310 minus one passes everything
bool partAllowed(const PartRow& row, uint32_t driverKey, uint32_t kartKey);

// room craft sub tab 0x010C rule FUN 00418D20
int roomCraftCategoryOf(int sub);

// car craft sub tab 0x0108 rule FUN 0041A3C0
int carCraftCategoryOf(int sub);

// the name key prefix of a car craft category COVER BOOSTER TIRES F FENDER R FENDER BUMPER WING
const char* carCraftCategoryPrefix(int category);

// car craft icon model name FUN 00442A40
std::string carCraftIcon(const std::string& chassis, int category, bool big = false);

}
