#include "ShopCommon.h"

#include "assets/AssetStore.h"

namespace KnC::Client {

namespace {

// a thousand separator so a six digit price stays readable
std::string grouped(uint32_t value) {
    std::string digits = std::to_string(value);
    std::string out;
    int seen = 0;
    for (size_t i = digits.size(); i > 0; --i) {
        out.push_back(digits[i - 1]);
        if (++seen % 3 == 0 && i > 1) out.push_back(' ');
    }
    std::string flipped(out.rbegin(), out.rend());
    return flipped;
}

}

// unit type 0 permanent 1 days 2 uses 3 durability the amount is silent on 0 and 3
std::string PriceQuote::unitText() const {
    switch (unitType) {
    case 0: return "permanent";
    case 1: return std::to_string(unitAmount) + (unitAmount == 1 ? " day" : " days");
    case 2: return std::to_string(unitAmount) + (unitAmount == 1 ? " use" : " uses");
    case 3: return "durability";
    default: return "unit " + std::to_string(unitType);
    }
}

std::string PriceQuote::priceText() const {
    if (!resolved) return "no price row";
    if (priceSale > 0) return grouped(priceSale) + " on sale from " + grouped(priceBase);
    return grouped(priceBase);
}

std::vector<PriceQuote> quotesOf(const Catalog& catalog, const std::vector<PriceOption>& options) {
    std::vector<PriceQuote> out;
    for (const PriceOption& o : options) {
        PriceQuote q;
        q.priceKey = o.priceKey;
        q.unitType = o.periodMode;
        q.unitAmount = o.periodValue;
        const PriceRow* row = catalog.price(o.priceKey);
        if (row) {
            q.unitType = row->unitType;
            q.unitAmount = row->unitAmount;
            q.priceBase = row->priceBase;
            q.priceSale = row->priceSale;
            q.resolved = true;
        }
        out.push_back(q);
    }
    return out;
}

namespace {
const char* iconSuffix(bool big) { return big ? "_02.png" : "_01.png"; }
}

std::string kartIcon(const KartRow& row, bool big) {
    return row.model.empty() ? std::string() : "Parts/kart_" + row.model + iconSuffix(big);
}

std::string driverIcon(const DriverRow& row, bool big) {
    return row.asset.empty() ? std::string() : "Parts/driver_" + row.asset + iconSuffix(big);
}

std::string itemIcon(const ItemRow& row, bool big) {
    return row.icon.empty() ? std::string() : "Parts/item_" + row.icon + iconSuffix(big);
}

std::string partIcon(const PartRow& row, bool big) {
    return row.model.empty() ? std::string() : "Parts/" + row.model + iconSuffix(big);
}

std::string petIcon(const PetRow& row, bool big) {
    return row.model.empty() ? std::string() : "Parts/pet_" + row.model + iconSuffix(big);
}

std::string pickIcon(AssetStore& assets, const std::string& first, const std::string& second) {
    if (!first.empty() && assets.texture(first) != nullptr) return first;
    if (!second.empty() && assets.texture(second) != nullptr) return second;
    return first;
}

std::vector<std::string> wrapText(const FontAtlas& font, const std::string& utf8, float px, float maxWidth) {
    std::vector<std::string> lines;
    std::string current;
    std::string word;
    auto flush = [&]() {
        if (!current.empty()) lines.push_back(current);
        current.clear();
    };
    auto push = [&]() {
        if (word.empty()) return;
        const std::string merged = current.empty() ? word : current + " " + word;
        if (!current.empty() && font.measure(merged, px) > maxWidth) {
            lines.push_back(current);
            current = word;
        } else {
            current = merged;
        }
        word.clear();
    };
    for (size_t i = 0; i < utf8.size(); ++i) {
        // the table stores a line break as the two characters backslash and n
        if (utf8[i] == '\\' && i + 1 < utf8.size() && utf8[i + 1] == 'n') {
            push();
            flush();
            ++i;
            continue;
        }
        if (utf8[i] == '\n') { push(); flush(); continue; }
        if (utf8[i] == ' ') { push(); continue; }
        word.push_back(utf8[i]);
    }
    push();
    flush();
    return lines;
}

bool partInTab(const PartRow& row, int category, int sub) {
    const uint32_t slot = row.equipSlot;
    if (category == 0) {
        if (sub == 1) return slot == 4;
        if (sub == 2) return slot == 2;
        if (sub == 3) return slot == 3 || slot == 5;
        if (sub == 4) return slot == 6;
        return false;
    }
    if (category == 1) {
        if (sub == 1) return slot == 8;
        if (sub == 2) return slot == 1;
        if (sub == 3) return slot == 0;
        return false;
    }
    return false;
}

bool partAllowed(const PartRow& row, uint32_t driverKey, uint32_t kartKey) {
    if (row.restrictKey == 0xFFFFFFFFu) return true;
    return row.restrictTarget == 0 ? row.restrictKey == driverKey : row.restrictKey == kartKey;
}

int roomCraftCategoryOf(int sub) {
    return sub >= 0 && sub <= 4 ? sub : -1;
}

int carCraftCategoryOf(int sub) {
    // FUN 00418D80 compares the record category with this table the sub tab order of the stock strip
    static const int kBySub[8] = {-1, 2, 0, 1, 3, 4, 5, 6};
    return sub >= 0 && sub < 8 ? kBySub[sub] : -1;
}

const char* carCraftCategoryPrefix(int category) {
    switch (category) {
    case 0: return "COVER";
    case 1: return "BOOSTER";
    case 2: return "TIRES";
    case 3: return "F_FENDER";
    case 4: return "R_FENDER";
    case 5: return "BUMPER";
    case 6: return "WING";
    default: return "";
    }
}

std::string carCraftIcon(const std::string& chassis, int category, bool big) {
    static const char* const kSuffix[7] = {"_cover", "_booster", "_tires", "_f_fender", "_r_fender",
                                           "_bumper", "_wing"};
    if (chassis.empty() || category < 0 || category > 6) return std::string();
    return "Parts/" + chassis + kSuffix[category] + "_B" + iconSuffix(big);
}

}
