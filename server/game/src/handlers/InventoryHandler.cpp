#include "handlers/InventoryHandler.h"
#include <map>
#include <string>
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/PartStatPackets.h"
#include "logging/Logger.h"
#include "db/Database.h"
#include "handlers/ProgressionHandler.h"
#include "util/GarageSlotRules.h"
#include "util/KartDurability.h"
#include "util/OwnedKartRow.h"

namespace knc {

void InventoryHandler::handleInventoryRequest(Session::Ptr session, GameServer* server) {
    (void)server;
    sendFullInventory(session);
}

void InventoryHandler::handleEquipVehicle(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    int32_t vehicleId = packet.readInt32();

    auto& db = Database::instance();

    // owned kart is the only kart truth and the id the client names is its row id
    auto karts = db.queryPrepared(
        ownedKartSelect() + "WHERE k.id = ? AND k.character_id = ?",
        {std::to_string(vehicleId), std::to_string(session->characterId)}
    );

    if (karts.empty()) {
        session->send(PacketBuilder::displayMessage(u"That kart is not in your garage.", 2));
        LOG_WARN("INVENTORY", "Equip vehicle: not found or not owned: id=" + std::to_string(vehicleId));
        return;
    }

    // one selection column holds the equipped kart so nothing has to be unequipped first
    db.executePrepared(
        "UPDATE characters SET selected_kart_instance_id = ? WHERE id = ?",
        {std::to_string(vehicleId), std::to_string(session->characterId)}
    );

    VehicleInfo vehicle = ownedKartVehicleInfo(karts[0]);
    vehicle.equipped = true;

    auto chars = db.queryPrepared(
        "SELECT gold, cash FROM characters WHERE id = ?",
        {std::to_string(session->characterId)}
    );
    int32_t gold = 0, cash = 0;
    if (!chars.empty()) {
        gold = std::stoi(chars[0]["gold"]);
        cash = std::stoi(chars[0]["cash"]);
    }

    session->send(PacketBuilder::equipVehicle(1, vehicleId, gold, cash, vehicle));
    LOG_INFO("INVENTORY", "Equipped vehicle " + std::to_string(vehicleId) +
             " for char " + std::to_string(session->characterId));
}

void InventoryHandler::handleEquipAccessory(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    int32_t accessoryId = packet.readInt32();
    int32_t slot = packet.readInt32();

    auto& db = Database::instance();

    auto accessories = db.queryPrepared(
        "SELECT id, accessory_type_id, bonus1, bonus2, bonus3 "
        "FROM accessories WHERE id = ? AND character_id = ?",
        {std::to_string(accessoryId), std::to_string(session->characterId)}
    );

    if (accessories.empty()) {
        session->send(PacketBuilder::displayMessage(u"That part is not in your garage.", 2));
        LOG_WARN("INVENTORY", "Equip accessory: not found or not owned: id=" + std::to_string(accessoryId));
        return;
    }

    // unequips any accessory already in this slot before equipping the new one
    db.executePrepared(
        "UPDATE accessories SET equipped = 0 WHERE character_id = ? AND slot = ?",
        {std::to_string(session->characterId), std::to_string(slot)}
    );
    db.executePrepared(
        "UPDATE accessories SET equipped = 1, slot = ? WHERE id = ? AND character_id = ?",
        {std::to_string(slot), std::to_string(accessoryId), std::to_string(session->characterId)}
    );

    AccessoryInfo acc;
    acc.id = accessoryId;
    acc.templateId = std::stoi(accessories[0]["accessory_type_id"]);
    acc.slot = slot;
    acc.bonus1 = std::stoi(accessories[0]["bonus1"]);
    acc.bonus2 = std::stoi(accessories[0]["bonus2"]);
    acc.bonus3 = std::stoi(accessories[0]["bonus3"]);
    acc.equipped = true;

    auto chars = db.queryPrepared(
        "SELECT gold, cash FROM characters WHERE id = ?",
        {std::to_string(session->characterId)}
    );
    int32_t gold = 0, cash = 0;
    if (!chars.empty()) {
        gold = std::stoi(chars[0]["gold"]);
        cash = std::stoi(chars[0]["cash"]);
    }

    session->send(PacketBuilder::equipAccessoryFull(2, accessoryId, gold, cash, acc));
    LOG_INFO("INVENTORY", "Equipped accessory " + std::to_string(accessoryId) +
             " to slot " + std::to_string(slot) +
             " for char " + std::to_string(session->characterId));
}

void InventoryHandler::handleUseItem(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    int32_t itemId = packet.readInt32();
    
    auto& db = Database::instance();

    auto items = db.queryPrepared("SELECT id, template_id, quantity FROM items WHERE id = ? AND character_id = ?",
                                  {std::to_string(itemId), std::to_string(session->characterId)});
    
    if (items.empty()) {
        LOG_WARN("INVENTORY", "Item not found: " + std::to_string(itemId));
        return;
    }
    
    int quantity = std::stoi(items[0]["quantity"]);
    
    if (quantity <= 1) {
        db.executePrepared("DELETE FROM items WHERE id = ?", {std::to_string(itemId)});
        session->send(PacketBuilder::removeItem(itemId, 0));
    } else {
        db.executePrepared("UPDATE items SET quantity = quantity - 1 WHERE id = ?", {std::to_string(itemId)});
        
        ItemInfo item;
        item.id = itemId;
        item.templateId = std::stoi(items[0]["template_id"]);
        item.quantity = quantity - 1;
        session->send(PacketBuilder::itemUpdate(item));
    }
    
    LOG_INFO("INVENTORY", "Used item " + std::to_string(itemId));
}

void InventoryHandler::handleSellItem(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    int32_t instanceId = packet.readInt32();
    int32_t quantity = packet.readInt32();
    int32_t itemType = packet.readInt32();    // 0 is item 1 is vehicle 2 is accessory
    
    if (quantity <= 0) {
        session->send(PacketBuilder::displayMessage(u"Invalid quantity", 0));
        return;
    }
    
    auto& db = Database::instance();
    
    int32_t templateId = 0;
    int32_t currentQty = 1;
    bool isEquipped = false;
    std::string tableName;
    std::string templateColumn;
    
    if (itemType == 1) {
        tableName = "owned_kart";
        templateColumn = "base_key";
        auto items = db.queryPrepared(
            "SELECT k.base_key, "
            "CASE WHEN c.selected_kart_instance_id = k.id THEN 1 ELSE 0 END AS equipped "
            "FROM owned_kart k JOIN characters c ON c.id = k.character_id "
            "WHERE k.id = ? AND k.character_id = ?",
            {std::to_string(instanceId), std::to_string(session->characterId)});
        if (items.empty()) {
            session->send(PacketBuilder::displayMessage(u"Vehicle not found", 0));
            return;
        }
        templateId = std::stoi(items[0]["base_key"]);
        isEquipped = std::stoi(items[0]["equipped"]) != 0;
        currentQty = 1;  // Vehicles don't stack

    } else if (itemType == 2) {
        tableName = "accessories";
        templateColumn = "accessory_type_id";
        auto items = db.queryPrepared(
            "SELECT accessory_type_id, equipped FROM accessories WHERE id = ? AND character_id = ?",
            {std::to_string(instanceId), std::to_string(session->characterId)});
        if (items.empty()) {
            session->send(PacketBuilder::displayMessage(u"Accessory not found", 0));
            return;
        }
        templateId = std::stoi(items[0]["accessory_type_id"]);
        isEquipped = std::stoi(items[0]["equipped"]) != 0;
        currentQty = 1;  // Accessories don't stack

    } else {
        tableName = "items";
        templateColumn = "item_type_id";
        auto items = db.queryPrepared(
            "SELECT item_type_id, quantity, slot FROM items WHERE id = ? AND character_id = ?",
            {std::to_string(instanceId), std::to_string(session->characterId)});
        if (items.empty()) {
            session->send(PacketBuilder::displayMessage(u"Item not found", 0));
            return;
        }
        templateId = std::stoi(items[0]["item_type_id"]);
        currentQty = std::stoi(items[0]["quantity"]);
        int32_t slot = std::stoi(items[0]["slot"]);
        isEquipped = (slot >= 0);  // a non-negative slot means the item is equipped
    }
    
    if (isEquipped) {
        session->send(PacketBuilder::displayMessage(u"Cannot sell equipped item", 0));
        return;
    }
    
    int32_t sellQty = std::min(quantity, currentQty);
    
    // sell price is 50 percent of the buy price 500 is the default fallback
    int32_t basePrice = 500;
    auto shopItems = db.queryPrepared("SELECT price_gold FROM shop_items WHERE template_id = ? LIMIT 1",
                                      {std::to_string(templateId)});
    if (!shopItems.empty()) {
        int32_t buyPrice = std::stoi(shopItems[0]["price_gold"]);
        basePrice = buyPrice / 2;
    }
    if (sellQty > 0 && basePrice > INT32_MAX / sellQty) {
        session->send(PacketBuilder::displayMessage(u"Invalid quantity", 0));
        return;
    }
    int32_t sellPrice = basePrice * sellQty;
    
    db.executePrepared("UPDATE characters SET gold = gold + ? WHERE id = ?",
                       {std::to_string(sellPrice), std::to_string(session->characterId)});
    
    // tableName is controlled to owned kart accessories or items only
    if (sellQty >= currentQty) {
        db.executePrepared("DELETE FROM " + tableName + " WHERE id = ?",
                           {std::to_string(instanceId)});
        session->send(PacketBuilder::removeItem(instanceId, itemType));
    } else {
        db.executePrepared("UPDATE " + tableName + " SET quantity = quantity - ? WHERE id = ?",
                           {std::to_string(sellQty), std::to_string(instanceId)});
    }
    
    // logs the sale with a prepared statement to prevent injection
    db.executePrepared(
        "INSERT INTO purchase_log (character_id, item_template_id, quantity, total_gold, ip_address) VALUES (?, ?, ?, ?, ?)",
        {std::to_string(session->characterId), std::to_string(-templateId),
         std::to_string(-sellQty), std::to_string(sellPrice), session->remoteAddress()});
    
    // the new balance goes out on 0x0A not 0x6A which sub 47AFE0 reads as a motion block
    ProgressionHandler::refreshPlayerProgress(session, server);
    
    std::string typeNames[] = {"item", "vehicle", "accessory"};
    int typeIdx = (itemType >= 0 && itemType <= 2) ? itemType : 0;
    LOG_INFO("INVENTORY", "Sold " + typeNames[typeIdx] + " template=" + std::to_string(templateId) +
             " x" + std::to_string(sellQty) + " for " + std::to_string(sellPrice) + "g");
}

void InventoryHandler::sendFullInventory(Session::Ptr session) {
    sendVehicleList(session);
    sendItemList(session);
    sendAccessoryList(session);
}

void InventoryHandler::sendVehicleList(Session::Ptr session) {
    // 0x1C is a full replace so one builder only the owned kart list the login burst also sends
    session->send(ownedKartListPacket(static_cast<int32_t>(session->characterId)));
    LOG_DEBUG("INVENTORY", "Sent the owned kart list for char " +
              std::to_string(session->characterId));
}

void InventoryHandler::sendItemList(Session::Ptr session) {
    auto items = loadItems(session->characterId);
    session->send(PacketBuilder::inventoryItems(items));
    LOG_DEBUG("INVENTORY", "Sent " + std::to_string(items.size()) + " items");
}

void InventoryHandler::sendAccessoryList(Session::Ptr session) {
    auto accessories = loadAccessories(session->characterId);
    session->send(PacketBuilder::inventoryAccessories(accessories));
    LOG_DEBUG("INVENTORY", "Sent " + std::to_string(accessories.size()) + " accessories");
}

std::vector<ItemInfo> InventoryHandler::loadItems(int32_t characterId) {
    std::vector<ItemInfo> items;
    auto& db = Database::instance();

    auto rows = db.queryPrepared(
        "SELECT id, item_type_id, quantity, slot, COALESCE(equipped, 0) AS equipped "
        "FROM items WHERE character_id = ?",
        {std::to_string(characterId)}
    );

    for (const auto& row : rows) {
        ItemInfo item;
        item.id = std::stoi(row.at("id"));
        item.templateId = std::stoi(row.at("item_type_id"));
        item.quantity = std::stoi(row.at("quantity"));
        item.slot = std::stoi(row.at("slot"));
        item.equipped = row.at("equipped") == "1";
        items.push_back(item);
    }

    return items;
}

std::vector<AccessoryInfo> InventoryHandler::loadAccessories(int32_t characterId) {
    std::vector<AccessoryInfo> accessories;
    auto& db = Database::instance();
    
    auto rows = db.queryPrepared(
        "SELECT id, accessory_type_id, slot, bonus1, bonus2, bonus3, equipped "
        "FROM accessories WHERE character_id = ?",
        {std::to_string(characterId)}
    );
    
    for (const auto& row : rows) {
        AccessoryInfo acc;
        acc.id = std::stoi(row.at("id"));
        acc.templateId = std::stoi(row.at("accessory_type_id"));
        acc.slot = std::stoi(row.at("slot"));
        acc.bonus1 = std::stoi(row.at("bonus1"));
        acc.bonus2 = std::stoi(row.at("bonus2"));
        acc.bonus3 = std::stoi(row.at("bonus3"));
        acc.equipped = row.at("equipped") == "1";
        accessories.push_back(acc);
    }
    
    return accessories;
}


namespace {

// the one place that knows the owned kart column order keeping both readers in step
void fillKart(const std::map<std::string, std::string>& r, InventoryPackets::KartRow& k) {
    auto num = [&r](const char* c) -> uint32_t {
        auto it = r.find(c);
        return it == r.end() || it->second.empty() ? 0u
                                                   : static_cast<uint32_t>(std::stoul(it->second));
    };
    k.instanceId    = num("id");
    k.baseKey       = num("base_key");
    k.skinPrimary   = num("skin_primary");
    k.skinSecondary = num("skin_secondary");
    k.skinTertiary  = num("skin_tertiary");
    k.custom3       = num("custom3");
    k.custom4       = num("custom4");
    k.custom5       = num("custom5");
    k.appliedItemA  = num("applied_item_a");
    k.appliedItemB  = num("applied_item_b");
    k.priceKey      = num("price_key");
    k.periodMode    = num("period_mode");
    k.periodValue   = num("period_value");
    k.activeFlag    = num("active_flag");
}

void fillChar(const std::map<std::string, std::string>& r, InventoryPackets::CharacterRow& c) {
    auto num = [&r](const char* col) -> int32_t {
        auto it = r.find(col);
        return it == r.end() || it->second.empty() ? 0 : std::stoi(it->second);
    };
    c.instanceId = static_cast<uint32_t>(num("id"));
    c.baseKey    = static_cast<uint32_t>(num("base_key"));
    c.accBody    = num("acc_body");
    c.accFace    = num("acc_face");
    c.accHead    = num("acc_head");
    c.accGlass   = num("acc_glass");
    c.accBack    = num("acc_back");
    c.priceKey   = static_cast<uint32_t>(num("price_key"));
    c.periodMode = static_cast<uint32_t>(num("period_mode"));
    c.periodValue= static_cast<uint32_t>(num("period_value"));
    c.activeFlag = static_cast<uint32_t>(num("active_flag"));
}

}  // namespace

bool InventoryHandler::selectedKartRow(int32_t characterId, InventoryPackets::KartRow& out) {
    auto& db = Database::instance();

    // the client persisted selection wins then the active flag then anything
    auto sel = db.queryPrepared(
        "SELECT k.* FROM owned_kart k JOIN characters c ON c.selected_kart_instance_id = k.id "
        "WHERE c.id = ? AND k.character_id = ? LIMIT 1",
        {characterId, characterId});
    if (sel.empty()) {
        sel = db.queryPrepared(
            "SELECT * FROM owned_kart WHERE character_id = ? AND active_flag = 1 "
            "ORDER BY id LIMIT 1", {characterId});
    }
    if (sel.empty()) {
        sel = db.queryPrepared(
            "SELECT * FROM owned_kart WHERE character_id = ? ORDER BY id LIMIT 1", {characterId});
    }
    if (sel.empty()) {
        LOG_WARN("INVENTORY", "char " + std::to_string(characterId) +
                 " owns no kart so the lobby stand and every room blob come up empty");
        return false;
    }
    fillKart(sel[0], out);
    return out.baseKey != 0;
}

// sub 4A5ED0 and sub 48C680 need all BODYSET slots or the chibi is invisible heal on read not just login
void healCharacterSlots(int32_t characterId, int32_t instanceId, int32_t baseKey) {
    auto& db = Database::instance();

    // def kart skin names and driver names differ only in case the default collation makes LIKE match them
    auto asset = db.queryPrepared("SELECT name FROM drivers WHERE id = ? LIMIT 1", {baseKey});
    if (asset.empty()) return;
    const std::string a = asset[0].at("name");

    static const char* kPart[3] = {"body", "face", "head"};
    static const char* kCol[3]  = {"acc_body", "acc_face", "acc_head"};
    for (int i = 0; i < 3; ++i) {
        auto sk = db.queryPrepared(
            "SELECT skin_key FROM def_kart_skin WHERE category = 2 AND name LIKE ? "
            "ORDER BY skin_key LIMIT 1",
            {a + "\_char\_" + kPart[i] + "\_%"});
        if (sk.empty()) continue;
        db.executePrepared(std::string("UPDATE owned_character SET ") + kCol[i] +
                               " = ? WHERE id = ? AND character_id = ? AND " + kCol[i] + " = 0",
                           {std::stoi(sk[0].at("skin_key")), instanceId, characterId});
    }
    LOG_INFO("INVENTORY", "healed empty BODYSET slots on owned_character " +
             std::to_string(instanceId) + " asset " + a);
}

bool InventoryHandler::selectedCharacterRow(int32_t characterId,
                                            InventoryPackets::CharacterRow& out) {
    auto& db = Database::instance();

    // equipped driver id is a base key not an instance id match on base key
    auto sel = db.queryPrepared(
        "SELECT o.* FROM owned_character o JOIN characters c ON c.equipped_driver_id = o.base_key "
        "WHERE c.id = ? AND o.character_id = ? LIMIT 1",
        {characterId, characterId});
    if (sel.empty()) {
        sel = db.queryPrepared(
            "SELECT * FROM owned_character WHERE character_id = ? AND active_flag = 1 "
            "ORDER BY id LIMIT 1", {characterId});
    }
    if (sel.empty()) {
        sel = db.queryPrepared(
            "SELECT * FROM owned_character WHERE character_id = ? ORDER BY id LIMIT 1",
            {characterId});
    }
    if (sel.empty()) {
        LOG_WARN("INVENTORY", "char " + std::to_string(characterId) + " owns no character row");
        return false;
    }
    fillChar(sel[0], out);

    // a bare bone means an invisible chibi so never publish a zero slot
    if (out.baseKey != 0 && (out.accBody == 0 || out.accFace == 0 || out.accHead == 0)) {
        healCharacterSlots(characterId, static_cast<int32_t>(out.instanceId),
                           static_cast<int32_t>(out.baseKey));
        auto again = db.queryPrepared("SELECT * FROM owned_character WHERE id = ? LIMIT 1",
                                      {static_cast<int32_t>(out.instanceId)});
        if (!again.empty()) fillChar(again[0], out);
    }
    return out.baseKey != 0;
}

Packet InventoryHandler::ownedKartListPacket(int32_t characterId) {
    // 0x1C is a full replace so a resend after a race puts the worn durability in the client copy
    std::vector<InventoryPackets::KartRow> karts;
    for (const auto& r : Database::instance().queryPrepared(
             "SELECT * FROM owned_kart WHERE character_id = ? ORDER BY id", {characterId})) {
        InventoryPackets::KartRow kr;
        fillKart(r, kr);
        karts.push_back(kr);
    }
    return InventoryPackets::ownedKartList(karts);
}

void InventoryHandler::buildLoginBurst(int32_t characterId, std::vector<Packet>& out) {
    // 0x1B and 0x1C are full replace containers sent once at login only
    if (characterId == 0) return;
    const int32_t charId = characterId;

    auto& db = Database::instance();

    std::vector<InventoryPackets::CharacterRow> chars;
    for (const auto& r : db.queryPrepared(
             "SELECT * FROM owned_character WHERE character_id = ? ORDER BY id", {charId})) {
        InventoryPackets::CharacterRow cr;
        fillChar(r, cr);
        chars.push_back(cr);
    }
    // always send the accessory list even empty an absent list is not the same as an empty one
    out.push_back(InventoryPackets::ownedCharacterList(chars));

    out.push_back(ownedKartListPacket(charId));

    auto num = [](const std::map<std::string, std::string>& r, const char* c) -> uint32_t {
        auto it = r.find(c);
        return it == r.end() || it->second.empty() ? 0u
                                                   : static_cast<uint32_t>(std::stoul(it->second));
    };

    std::vector<InventoryPackets::ItemRow> items;
    for (const auto& r : db.queryPrepared(
             "SELECT * FROM owned_item WHERE character_id = ? ORDER BY id", {charId})) {
        InventoryPackets::ItemRow ir;
        ir.instanceId  = num(r, "id");
        ir.baseKey     = num(r, "base_key");
        ir.periodMode  = num(r, "period_mode");
        ir.periodValue = static_cast<int32_t>(num(r, "period_value"));
        ir.activeFlag  = num(r, "active_flag");
        ir.inUseFlag   = static_cast<int32_t>(num(r, "in_use_flag"));
        items.push_back(ir);
    }
    out.push_back(InventoryPackets::ownedItemList(items));

    for (const auto& r : db.queryPrepared(
             "SELECT * FROM owned_part WHERE character_id = ? ORDER BY id", {charId})) {
        InventoryPackets::PartRow pr;
        pr.instanceId  = num(r, "id");
        pr.baseKey     = num(r, "base_key");
        pr.periodMode  = num(r, "period_mode");
        pr.periodValue = num(r, "period_value");
        pr.activeFlag  = num(r, "active_flag");
        out.push_back(InventoryPackets::ownedPartRecord(pr));
    }

    std::vector<InventoryPackets::PetRow> pets;
    for (const auto& r : db.queryPrepared(
             "SELECT * FROM owned_pet WHERE character_id = ? ORDER BY id", {charId})) {
        InventoryPackets::PetRow pe;
        pe.instanceId   = num(r, "id");
        pe.baseKey      = num(r, "base_key");
        pe.equippedFlag = num(r, "equipped_flag");
        pe.periodMode   = num(r, "period_mode");
        pe.periodValue  = num(r, "period_value");
        pe.activeFlag   = num(r, "active_flag");
        pets.push_back(pe);
    }
    // always send the owned pet list even empty a later empty stub would wipe a real one on the client
    out.push_back(InventoryPackets::ownedPetList(pets));

    LOG_INFO("INVENTORY", "login burst " + std::to_string(out.size()) +
             " owned containers for char " + std::to_string(charId));
}

// garage verbs 0xB8 0xB9 0xBA were lost and stubbed back the wire key is a base key not a row

namespace {

// slot resolution from client sub 484B10 uiCategory at partDef offset 0x38 picks the target field
struct SkinTarget {
    const char* table = nullptr;
    const char* column = nullptr;
};

SkinTarget skinTargetFor(uint32_t skinKey) {
    auto& db = Database::instance();
    auto rows = db.queryPrepared(
        "SELECT category, name FROM def_kart_skin WHERE skin_key = ? LIMIT 1",
        {static_cast<int32_t>(skinKey)});
    if (rows.empty()) return SkinTarget{};

    // the same map the 0xC2 burst sends or a glass and a back part answer MSG UNKNOWN ERROR
    switch (partSlotFor(std::stoi(rows[0].at("category")), rows[0].at("name"))) {
    case 0: return SkinTarget{"owned_kart", "skin_primary"};
    case 1: return SkinTarget{"owned_kart", "skin_secondary"};
    case 8: return SkinTarget{"owned_kart", "skin_tertiary"};
    case 2: return SkinTarget{"owned_character", "acc_body"};
    case 3: return SkinTarget{"owned_character", "acc_face"};
    case 4: return SkinTarget{"owned_character", "acc_head"};
    case 5: return SkinTarget{"owned_character", "acc_glass"};
    case 6: return SkinTarget{"owned_character", "acc_back"};
    default: return SkinTarget{};
    }
}

int32_t colInt(const std::map<std::string, std::string>& r, const char* c) {
    auto it = r.find(c);
    return it == r.end() || it->second.empty() ? 0 : std::stoi(it->second);
}

int64_t colI64(const std::map<std::string, std::string>& r, const char* c) {
    auto it = r.find(c);
    return it == r.end() || it->second.empty() ? 0 : std::stoll(it->second);
}

// a part leaving the kart puts the kart def key back so the kart builder never meets zero paint
bool resetWornPart(int32_t charId, uint32_t partKey, bool onlySelected, int32_t selectedKartId) {
    const SkinTarget tgt = skinTargetFor(partKey);
    if (tgt.table == nullptr) return false;
    auto& db = Database::instance();
    const std::string col = tgt.column;
    if (std::string(tgt.table) == "owned_kart") {
        const int32_t back = static_cast<int32_t>(kartSkinDefaultKey(col));
        if (onlySelected) {
            return db.executePrepared("UPDATE owned_kart SET " + col + " = ? WHERE id = ? AND character_id = ? AND " +
                                          col + " = ?",
                                      {back, selectedKartId, charId, static_cast<int32_t>(partKey)});
        }
        return db.executePrepared("UPDATE owned_kart SET " + col + " = ? WHERE character_id = ? AND " + col + " = ?",
                                  {back, charId, static_cast<int32_t>(partKey)});
    }
    // a zero body face or head heals on the next read of selectedCharacterRow
    return db.executePrepared("UPDATE owned_character SET " + col + " = 0 WHERE character_id = ? AND " + col + " = ?",
                              {charId, static_cast<int32_t>(partKey)});
}

// the scroll price row of unit type 3 sells its durability amount no such row means a full bar
int32_t repairAmountFor(int32_t itemKey) {
    auto rows = Database::instance().queryPrepared(
        "SELECT p.unit_amount FROM shop_option o JOIN shop_price p ON p.price_key = o.price_key "
        "WHERE o.category = 2 AND o.base_key = ? AND p.unit_type = 3 ORDER BY o.slot LIMIT 1",
        {itemKey});
    if (rows.empty()) return KART_REPAIR_DEFAULT_AMOUNT;
    const int32_t amount = colInt(rows[0], "unit_amount");
    return amount > 0 ? amount : KART_REPAIR_DEFAULT_AMOUNT;
}

}  // namespace

bool InventoryHandler::handleDelete(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    if (!session || session->characterId == 0) return false;

    InventoryPackets::DeleteRequest req;
    if (!InventoryPackets::parseDelete(packet, req)) return false;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    auto& db = Database::instance();

    static const char* kTable[5] = {
        "owned_character", "owned_kart", "owned_item", "owned_part", "owned_pet"};
    if (req.category > 4) {
        LOG_WARN("INVENTORY", "delete category " + std::to_string(req.category) + " unknown");
        return false;
    }
    const std::string table = kTable[req.category];

    // Delete shows only on a row the client saw expire sub 451C90 clears the active flag on its own clock
    auto rows = db.queryPrepared(
        "SELECT id, active_flag, period_mode, period_value FROM " + table +
        " WHERE character_id = ? AND base_key = ? ORDER BY active_flag ASC, id ASC LIMIT 1",
        {charId, static_cast<int32_t>(req.baseKey)});
    const int64_t today = clientDayNumberToday();
    if (rows.empty() ||
        !ownedRowExpired(static_cast<uint32_t>(colInt(rows[0], "active_flag")),
                         static_cast<uint32_t>(colInt(rows[0], "period_mode")),
                         colI64(rows[0], "period_value"), today)) {
        LOG_WARN("INVENTORY", "delete cat " + std::to_string(req.category) + " key " +
                 std::to_string(req.baseKey) + " refused the row is missing or still running char " +
                 std::to_string(charId));
        session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
        return true;
    }

    db.executePrepared("DELETE FROM " + table + " WHERE id = ? AND character_id = ?",
                       {colInt(rows[0], "id"), charId});
    const bool worn = req.category == InventoryPackets::CAT_PART &&
                      resetWornPart(charId, req.baseKey, false, 0);
    session->send(InventoryPackets::deleteAck(req.category, req.baseKey));
    // sub 484690 only drops the owned row so the kart record with its default paint comes after
    if (worn) sendEquipmentSet(session);
    LOG_INFO("INVENTORY", "delete cat " + std::to_string(req.category) + " key " +
             std::to_string(req.baseKey) + " char " + std::to_string(charId) +
             (worn ? " worn slot back to its default" : ""));
    return true;
}

bool InventoryHandler::handleInstall(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    if (!session || session->characterId == 0) return false;

    InventoryPackets::InstallRequest req;
    if (!InventoryPackets::parseInstall(packet, req)) return false;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    const int32_t key = static_cast<int32_t>(req.baseKey);
    auto& db = Database::instance();

    switch (req.category) {
    case InventoryPackets::CAT_CHARACTER: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_character WHERE character_id = ? AND base_key = ? LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
            return true;
        }
        // equipped driver id holds the base key selectedCharacterRow joins on it
        db.executePrepared("UPDATE characters SET equipped_driver_id = ? WHERE id = ?",
                           {key, charId});
        InventoryPackets::CharacterRow cr;
        fillChar(rows[0], cr);
        // the 0xB9 ack must carry the healed slots not the row read before the heal ran
        if (cr.accBody == 0 || cr.accFace == 0 || cr.accHead == 0) {
            healCharacterSlots(charId, static_cast<int32_t>(cr.instanceId), key);
            auto again = db.queryPrepared("SELECT * FROM owned_character WHERE id = ? LIMIT 1",
                                          {static_cast<int32_t>(cr.instanceId)});
            if (!again.empty()) fillChar(again[0], cr);
        }
        session->send(InventoryPackets::selectCharacterAck(cr));
        sendEquipmentSet(session);
        LOG_INFO("INVENTORY", "select char base " + std::to_string(key) + " inst " +
                 std::to_string(cr.instanceId) + " char " + std::to_string(charId));
        return true;
    }

    case InventoryPackets::CAT_KART: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_kart WHERE character_id = ? AND base_key = ? "
            "ORDER BY active_flag DESC, id ASC LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
            return true;
        }
        InventoryPackets::KartRow kr;
        fillKart(rows[0], kr);
        // selected kart instance id is a row id not a base key
        db.executePrepared("UPDATE characters SET selected_kart_instance_id = ? WHERE id = ?",
                           {static_cast<int32_t>(kr.instanceId), charId});
        session->send(InventoryPackets::selectKartAck(kr));
        sendEquipmentSet(session);
        LOG_INFO("INVENTORY", "select kart base " + std::to_string(key) + " inst " +
                 std::to_string(kr.instanceId) + " char " + std::to_string(charId));
        return true;
    }

    case InventoryPackets::CAT_ITEM: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_item WHERE character_id = ? AND base_key = ? "
            "ORDER BY id ASC LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
            return true;
        }
        InventoryPackets::ItemRow ir;
        ir.instanceId  = static_cast<uint32_t>(colInt(rows[0], "id"));
        ir.baseKey     = static_cast<uint32_t>(colInt(rows[0], "base_key"));
        ir.priceKey    = static_cast<uint32_t>(colInt(rows[0], "price_key"));
        ir.periodMode  = static_cast<uint32_t>(colInt(rows[0], "period_mode"));
        ir.periodValue = colInt(rows[0], "period_value");
        ir.activeFlag  = static_cast<uint32_t>(colInt(rows[0], "active_flag"));
        ir.inUseFlag   = colInt(rows[0], "in_use_flag");

        // a 0xC1 use type 4 to 7 is a repair scroll 0x412BC0 sends on a mode 3 kart
        uint32_t useType = 0;
        if (PartStatPackets::loadItemUseType(static_cast<uint32_t>(key), useType) &&
            PartStatPackets::isRepairItemType(useType)) {
            // the ack of this def type carries the kart period tail so the selected kart must exist on mode 3
            InventoryPackets::KartRow kr;
            if (!selectedKartRow(charId, kr) || kr.periodMode != KART_PERIOD_DURABILITY) {
                session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
                return true;
            }
            if (ir.periodMode == InventoryPackets::PERIOD_COUNT && ir.periodValue <= 0) {
                session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
                return true;
            }
            const int32_t amount = repairAmountFor(key);
            int32_t next = static_cast<int32_t>(kr.periodValue);
            kartDurabilityAfterRepair(kr.periodMode, kr.periodValue, amount, next);
            // a full bar eats no scroll the ack still closes the box with the values unchanged
            if (next != static_cast<int32_t>(kr.periodValue)) {
                db.executePrepared(
                    "UPDATE owned_kart SET period_value = ? WHERE id = ? AND character_id = ?",
                    {next, static_cast<int32_t>(kr.instanceId), charId});
                kr.periodValue = static_cast<uint32_t>(next);
                if (ir.periodMode == InventoryPackets::PERIOD_COUNT) {
                    db.executePrepared(
                        "UPDATE owned_item SET period_value = period_value - 1 "
                        "WHERE id = ? AND character_id = ? AND period_value > 0",
                        {static_cast<int32_t>(ir.instanceId), charId});
                    ir.periodValue -= 1;
                }
            }
            session->send(InventoryPackets::useItemAckWithKartPeriod(ir, kr));
            LOG_INFO("INVENTORY", "repair scroll " + std::to_string(key) + " kart inst " +
                     std::to_string(kr.instanceId) + " durability " +
                     std::to_string(kr.periodValue) + " char " + std::to_string(charId));
            return true;
        }

        ir.inUseFlag = 1;
        db.executePrepared("UPDATE owned_item SET in_use_flag = 1 WHERE id = ? AND character_id = ?",
                           {static_cast<int32_t>(ir.instanceId), charId});
        session->send(InventoryPackets::useItemAck(ir));
        LOG_INFO("INVENTORY", "use item base " + std::to_string(key) + " char " +
                 std::to_string(charId));
        return true;
    }

    case InventoryPackets::CAT_PART: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_part WHERE character_id = ? AND base_key = ? "
            "ORDER BY id ASC LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
            return true;
        }
        InventoryPackets::PartRow pr;
        pr.instanceId  = static_cast<uint32_t>(colInt(rows[0], "id"));
        pr.baseKey     = static_cast<uint32_t>(colInt(rows[0], "base_key"));
        pr.unk08       = static_cast<uint32_t>(colInt(rows[0], "unk_08"));
        pr.priceKey    = static_cast<uint32_t>(colInt(rows[0], "price_key"));
        pr.periodMode  = static_cast<uint32_t>(colInt(rows[0], "period_mode"));
        pr.periodValue = static_cast<uint32_t>(colInt(rows[0], "period_value"));
        pr.activeFlag  = static_cast<uint32_t>(colInt(rows[0], "active_flag"));

        // the part only shows up once its key sits in the owned row the stand reads
        const SkinTarget tgt = skinTargetFor(req.baseKey);
        if (tgt.table == nullptr) {
            LOG_WARN("INVENTORY", "part key " + std::to_string(key) + " has no skin slot");
            session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
            return true;
        }
        if (std::string(tgt.table) == "owned_kart") {
            InventoryPackets::KartRow kr;
            if (!selectedKartRow(charId, kr)) {
                session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
                return true;
            }
            db.executePrepared(std::string("UPDATE owned_kart SET ") + tgt.column +
                                   " = ? WHERE id = ? AND character_id = ?",
                               {key, static_cast<int32_t>(kr.instanceId), charId});
        } else {
            InventoryPackets::CharacterRow cr;
            if (!selectedCharacterRow(charId, cr)) {
                session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
                return true;
            }
            db.executePrepared(std::string("UPDATE owned_character SET ") + tgt.column +
                                   " = ? WHERE id = ? AND character_id = ?",
                               {key, static_cast<int32_t>(cr.instanceId), charId});
        }

        session->send(InventoryPackets::equipPartAck(pr));
        sendEquipmentSet(session);
        LOG_INFO("INVENTORY", "equip part " + std::to_string(key) + " into " +
                 std::string(tgt.table) + "." + tgt.column + " char " + std::to_string(charId));
        return true;
    }

    case InventoryPackets::CAT_PET: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_pet WHERE character_id = ? AND base_key = ? "
            "ORDER BY id ASC LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(PacketBuilder::messageKey("MSG_UNKNOWN_ERROR", 1));
            return true;
        }
        InventoryPackets::PetRow np;
        np.instanceId   = static_cast<uint32_t>(colInt(rows[0], "id"));
        np.baseKey      = static_cast<uint32_t>(colInt(rows[0], "base_key"));
        np.equippedFlag = 1;
        np.priceKey     = static_cast<uint32_t>(colInt(rows[0], "price_key"));
        np.periodMode   = static_cast<uint32_t>(colInt(rows[0], "period_mode"));
        np.periodValue  = static_cast<uint32_t>(colInt(rows[0], "period_value"));
        np.activeFlag   = static_cast<uint32_t>(colInt(rows[0], "active_flag"));

        // one pet out one pet in else the client keeps two lit at once
        auto prev = db.queryPrepared(
            "SELECT * FROM owned_pet WHERE character_id = ? AND equipped_flag = 1 AND id <> ? "
            "ORDER BY id ASC LIMIT 1",
            {charId, static_cast<int32_t>(np.instanceId)});
        db.executePrepared("UPDATE owned_pet SET equipped_flag = 0 WHERE character_id = ?",
                           {charId});
        db.executePrepared("UPDATE owned_pet SET equipped_flag = 1 WHERE id = ? AND character_id = ?",
                           {static_cast<int32_t>(np.instanceId), charId});

        if (prev.empty()) {
            session->send(InventoryPackets::equipPetAck(1, np));
        } else {
            InventoryPackets::PetRow op;
            op.instanceId   = static_cast<uint32_t>(colInt(prev[0], "id"));
            op.baseKey      = static_cast<uint32_t>(colInt(prev[0], "base_key"));
            op.equippedFlag = 0;
            op.priceKey     = static_cast<uint32_t>(colInt(prev[0], "price_key"));
            op.periodMode   = static_cast<uint32_t>(colInt(prev[0], "period_mode"));
            op.periodValue  = static_cast<uint32_t>(colInt(prev[0], "period_value"));
            op.activeFlag   = static_cast<uint32_t>(colInt(prev[0], "active_flag"));
            session->send(InventoryPackets::equipPetAckReplace(op, np));
        }
        LOG_INFO("INVENTORY", "equip pet " + std::to_string(key) + " char " +
                 std::to_string(charId));
        return true;
    }

    default:
        LOG_WARN("INVENTORY", "install category " + std::to_string(req.category) + " unhandled");
        return false;
    }
}

bool InventoryHandler::handleRemove(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;
    if (!session || session->characterId == 0) return false;

    InventoryPackets::RemoveRequest req;
    if (!InventoryPackets::parseRemove(packet, req)) return false;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    const int32_t key = static_cast<int32_t>(req.baseKey);
    auto& db = Database::instance();

    switch (req.category) {
    case InventoryPackets::CAT_ITEM: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_item WHERE character_id = ? AND base_key = ? LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(InventoryPackets::unequipNoopAck(InventoryPackets::CAT_CHARACTER));
            return true;
        }
        InventoryPackets::ItemRow ir;
        ir.instanceId  = static_cast<uint32_t>(colInt(rows[0], "id"));
        ir.baseKey     = static_cast<uint32_t>(colInt(rows[0], "base_key"));
        ir.priceKey    = static_cast<uint32_t>(colInt(rows[0], "price_key"));
        ir.periodMode  = static_cast<uint32_t>(colInt(rows[0], "period_mode"));
        ir.periodValue = colInt(rows[0], "period_value");
        ir.activeFlag  = static_cast<uint32_t>(colInt(rows[0], "active_flag"));
        ir.inUseFlag   = 0;
        db.executePrepared("UPDATE owned_item SET in_use_flag = 0 WHERE id = ? AND character_id = ?",
                           {static_cast<int32_t>(ir.instanceId), charId});
        session->send(InventoryPackets::unequipItemAck(ir));
        return true;
    }

    case InventoryPackets::CAT_PART: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_part WHERE character_id = ? AND base_key = ? LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(InventoryPackets::unequipNoopAck(InventoryPackets::CAT_CHARACTER));
            return true;
        }
        InventoryPackets::PartRow pr;
        pr.instanceId  = static_cast<uint32_t>(colInt(rows[0], "id"));
        pr.baseKey     = static_cast<uint32_t>(colInt(rows[0], "base_key"));
        pr.unk08       = static_cast<uint32_t>(colInt(rows[0], "unk_08"));
        pr.priceKey    = static_cast<uint32_t>(colInt(rows[0], "price_key"));
        pr.periodMode  = static_cast<uint32_t>(colInt(rows[0], "period_mode"));
        pr.periodValue = static_cast<uint32_t>(colInt(rows[0], "period_value"));
        pr.activeFlag  = static_cast<uint32_t>(colInt(rows[0], "active_flag"));

        // sub 484B10 puts the kart def paint plate or antenna back so the row takes the same key
        const SkinTarget tgt = skinTargetFor(req.baseKey);
        if (tgt.table != nullptr) {
            if (std::string(tgt.table) == "owned_kart") {
                InventoryPackets::KartRow kr;
                if (selectedKartRow(charId, kr)) {
                    resetWornPart(charId, req.baseKey, true, static_cast<int32_t>(kr.instanceId));
                }
            } else {
                InventoryPackets::CharacterRow cr;
                if (selectedCharacterRow(charId, cr)) {
                    db.executePrepared(std::string("UPDATE owned_character SET ") + tgt.column +
                                           " = 0 WHERE id = ? AND character_id = ? AND " +
                                           tgt.column + " = ?",
                                       {static_cast<int32_t>(cr.instanceId), charId, key});
                }
            }
        }
        session->send(InventoryPackets::unequipPartAck(pr));
        sendEquipmentSet(session);
        LOG_INFO("INVENTORY", "remove part " + std::to_string(key) + " char " + std::to_string(charId));
        return true;
    }

    case InventoryPackets::CAT_PET: {
        auto rows = db.queryPrepared(
            "SELECT * FROM owned_pet WHERE character_id = ? AND base_key = ? LIMIT 1",
            {charId, key});
        if (rows.empty()) {
            session->send(InventoryPackets::unequipNoopAck(InventoryPackets::CAT_CHARACTER));
            return true;
        }
        InventoryPackets::PetRow pe;
        pe.instanceId   = static_cast<uint32_t>(colInt(rows[0], "id"));
        pe.baseKey      = static_cast<uint32_t>(colInt(rows[0], "base_key"));
        pe.equippedFlag = 0;
        pe.priceKey     = static_cast<uint32_t>(colInt(rows[0], "price_key"));
        pe.periodMode   = static_cast<uint32_t>(colInt(rows[0], "period_mode"));
        pe.periodValue  = static_cast<uint32_t>(colInt(rows[0], "period_value"));
        pe.activeFlag   = static_cast<uint32_t>(colInt(rows[0], "active_flag"));
        db.executePrepared("UPDATE owned_pet SET equipped_flag = 0 WHERE id = ? AND character_id = ?",
                           {static_cast<int32_t>(pe.instanceId), charId});
        session->send(InventoryPackets::unequipPetAck(pe));
        return true;
    }

    default:
        // char and kart carry no record after the category else the frame desyncs
        session->send(InventoryPackets::unequipNoopAck(req.category));
        return true;
    }
}

void InventoryHandler::sendEquipmentSet(Session::Ptr session) {
    if (!session || session->characterId == 0) return;
    const int32_t charId = static_cast<int32_t>(session->characterId);

    InventoryPackets::CharacterRow cr;
    InventoryPackets::KartRow kr;
    if (!selectedCharacterRow(charId, cr) || !selectedKartRow(charId, kr)) return;

    session->send(InventoryPackets::applyEquipmentSet(cr.instanceId, kr.instanceId, cr, kr));
}

bool InventoryHandler::isItemStateNotify(const Packet& packet) const {
    // the notify carries an instance id a base key and three counters
    return packet.payload().size() >= 20;
}

void InventoryHandler::handleItemStateNotify(Session::Ptr session, Packet& packet,
                                             GameServer* server) {
    (void)packet; (void)server;
    if (!session) return;
}

} // namespace knc
