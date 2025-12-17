#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class GameServer;

class ShopHandler {
public:
    void handleEnterShop(Session::Ptr session, GameServer* server);
    void handleExitShop(Session::Ptr session, GameServer* server);
    void handleBrowse(Session::Ptr session, Packet& packet, GameServer* server);
    void handlePurchase(Session::Ptr session, Packet& packet, GameServer* server);
    void handleGift(Session::Ptr session, Packet& packet, GameServer* server);
    
    void sendShopList(Session::Ptr session, int32_t category);
    void sendPlayerCurrency(Session::Ptr session, int32_t gold, int32_t cash);
};

} // namespace knc

