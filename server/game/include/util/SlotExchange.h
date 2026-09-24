/// Slot Exchange rules sub 4AEDD0 swaps slot 0 and 1 on Alt for one use of item 1000

#pragma once

#include <cstdint>

namespace knc {

/// base key of the owned item row sub 4AEDD0 looks up for the swap
constexpr int32_t SLOT_EXCHANGE_ITEM_KEY = 1000;

/// sub 482F10 sends flag 1 after a swap sub 482DB0 sends flag 0 at the lobby or room init
constexpr uint8_t SLOT_EXCHANGE_FLAG_SYNC = 0;
constexpr uint8_t SLOT_EXCHANGE_FLAG_USED = 1;

/// what one C2S 0x00CB of the row does to the stored count
struct SlotExchangeStep {
    bool decrement = false;
    int32_t countAfter = 0;
    /// S2C 0x0115 puts countAfter back on the client row
    bool resync = false;
};

/// the client already took the use so the database follows it and any difference goes back on 0x0115
inline SlotExchangeStep slotExchangeStep(uint8_t flag, int32_t wireCount, int32_t wireActive,
                                         int32_t storedCount) {
    SlotExchangeStep s;
    s.countAfter = storedCount > 0 ? storedCount : 0;
    if (flag == SLOT_EXCHANGE_FLAG_USED && wireActive != 0 && s.countAfter > 0) {
        s.decrement = true;
        --s.countAfter;
    }
    s.resync = s.countAfter != wireCount;
    return s;
}

}  // namespace knc
