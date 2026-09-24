#pragma once
#include "packets/gen/ShopPackets.h"
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class GameServer;

class ShopHandler {
public:
    /// out collects the frames instead of sending so the login burst can build them off the loop
    static void sendLoginCatalogs(Session::Ptr session, GameServer* server,
                                  std::vector<Packet>* out = nullptr);
    static void handleShopEntryAlt(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleCashPoll(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleBuy(Session::Ptr session, const ShopPackets::BuyRequest& req, GameServer* server);
    static void handleSell(Session::Ptr session, const ShopPackets::SellRequest& req, GameServer* server);
    static void handleGift(Session::Ptr session, const ShopPackets::GiftRequest& req, GameServer* server);
    static void handleExtend(Session::Ptr session, const ShopPackets::ExtendRequest& req, GameServer* server);
    static void handleEnterShop(Session::Ptr session, GameServer* server);
    /// grants one owned row free reward types 0 to 6 match shop categories recordOut gets the record client appends
    static bool grantReward(uint32_t characterId, uint32_t category, uint32_t baseKey,
                            std::vector<uint8_t>& recordOut);
};

} // namespace knc

