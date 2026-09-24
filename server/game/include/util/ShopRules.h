/// the shop rules the stock client draws and the captured burst of the real server sold with

#pragma once

#include <cstdint>

namespace knc {

/// 0x45E36B draws UI Shop iteminfo buy Cash beside a price row whose sale is above zero and Gold else
inline bool priceIsAstro(int32_t priceSale) { return priceSale > 0; }

/// the amount one buy costs the sale in Astro when set else the base in gold
inline int32_t priceCharged(int32_t priceBase, int32_t priceSale) {
    return priceSale > 0 ? priceSale : priceBase;
}

/// sub 4571D0 tab zero reads the owned row of base key 2000 the Gacha Coin
constexpr uint32_t kGachaCoinKey = 2000;
/// sub 4571D0 tab one reads the owned row of base key 0x7D1 the Gold Coin
constexpr uint32_t kGoldCoinKey = 2001;

/// the two coins the gacha popup spends through C2S 0x00ED
inline bool isGachaCoinKey(uint32_t itemKey) {
    return itemKey == kGachaCoinKey || itemKey == kGoldCoinKey;
}

/// the owned item period pair of a coin row mode 2 counts the coins in value
struct CoinPeriod {
    uint32_t mode = 2;
    int64_t value = 0;
};

/// sub 4830C0 needs rec 0x14 set and rec 0x10 above zero so each coin buy adds to the count
inline CoinPeriod coinPeriodAfterPurchase(uint32_t oldMode, int64_t oldValue, uint32_t unitType,
                                          uint32_t unitAmount) {
    CoinPeriod p;
    const int64_t held = (oldMode == 2 && oldValue > 0) ? oldValue : 0;
    // the burst sells each coin on a unit 0 row which is one coin and unit 2 gives its amount
    const int64_t add = (unitType == 2 && unitAmount > 0) ? static_cast<int64_t>(unitAmount) : 1;
    p.mode = 2;
    p.value = held + add;
    return p;
}

/// burst price tier driver 1 kart 2 worn part 3 kart part 4 pet 5 room 6 craft 7
inline uint32_t burstPriceTier(uint32_t shopCategory, uint32_t equipSlot) {
    switch (shopCategory) {
        case 0: return 1;
        case 1: return 2;
        case 3: return (equipSlot >= 2 && equipSlot <= 6) ? 3u : 4u;
        case 4: return 5;
        case 5: return 6;
        case 6: return 7;
        default: return 0;
    }
}

/// the three option keys of a tier in the burst order one day seven days permanent
inline uint32_t burstDayKey(uint32_t tier) { return 2000 + tier; }
inline uint32_t burstWeekKey(uint32_t tier) { return 3000 + tier; }
inline uint32_t burstPermanentKey(uint32_t tier) { return 1000 + tier; }

/// the one option of each coin in the burst 4001 sale 1000 Astro and 4002 base 2500 gold both permanent
inline uint32_t burstCoinPriceKey(uint32_t itemKey) {
    if (itemKey == kGachaCoinKey) return 4001;
    if (itemKey == kGoldCoinKey) return 4002;
    return 0;
}

}  // namespace knc
