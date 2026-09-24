/// the login burst the catalogue sends after login and the drip sender that delivers them

#include "GameServer.h"
#include "GameServerInternal.h"
#include "packets/gen/GhostPackets.h"
#include "packets/gen/PartStatPackets.h"
#include "packets/gen/GachaPetPackets.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/ShopPackets.h"
#include "packets/gen/MissionPackets.h"
#include "packets/gen/SocialPackets.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/SpawnPackets.h"
#include "packets/gen/ItemPackets.h"
#include "handlers/ProgressionHandler.h"
#include "handlers/QuestHandler.h"
#include "logging/Logger.h"
#include <unordered_set>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <memory>
#include <functional>
#include <array>
#include <map>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <cctype>
#include "security/BanManager.h"
#include "security/PacketValidator.h"
#include "db/Database.h"
#include "handlers/LicenseHandler.h"
#include "handlers/MissionHandler.h"
#include "handlers/AntiCheatHandler.h"
#include "handlers/GarageHandler.h"
#include "handlers/LobbyHandler.h"
#include "handlers/GhostHandler.h"
#include "handlers/ScenarioHandler.h"
#include "handlers/CharCreateHandler.h"
#include "handlers/ItemHandler.h"
#include "handlers/GachaHandler.h"
#include "handlers/KeepaliveAnticheatHandler.h"
#include "crypto/PasswordHash.h"
#include "util/PendantRules.h"

namespace knc {

namespace {

// login redirect is an ascii cstr ip so the frame is 17 bytes plus the host length

// sub 476cc0 client recv buffer fixed 0x2000 so drip login chunks past one frame KNC DRIP GAP KNC DRIP CHUNK
static int dripGapMs() {
    static int v = [] {
        const char* e = getenv("KNC_DRIP_GAP");
        int n = (e && e[0]) ? atoi(e) : 25;
        return (n < 1 || n > 5000) ? 25 : n;
    }();
    return v;
}
// sub 476CC0 client recv buffer fixed 0x2000 login burst reached 39940 bytes KNC LAZY CATALOGS defers unneeded tables
static bool lazyCatalogs() {
    static bool v = [] {
        const char* e = getenv("KNC_LAZY_CATALOGS");
        return e && e[0] == '1';
    }();
    return v;
}

// hold the whole login burst back so the client can settle after the redirect
static int loginDelayMs() {
    static int v = [] {
        const char* e = getenv("KNC_LOGIN_DELAY_MS");
        int n = (e && e[0]) ? atoi(e) : 0;
        return (n < 0 || n > 20000) ? 0 : n;
    }();
    return v;
}
static size_t dripChunk() {
    static size_t v = [] {
        const char* e = getenv("KNC_DRIP_CHUNK");
        long n = (e && e[0]) ? atol(e) : 3500;
        return (n < 64 || n > 7000) ? size_t(3500) : size_t(n);
    }();
    return v;
}

/// milliseconds since a mark the burst logs the io thread cost and the pool cost apart
static double msSince(const std::chrono::steady_clock::time_point& t0) {
    const auto d = std::chrono::steady_clock::now() - t0;
    return std::chrono::duration<double, std::milli>(d).count();
}

/// two decimals so a sub millisecond io phase still reads as a number
static std::string fmtMs(double ms) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f", ms);
    return std::string(buf);
}

struct SpacedSender : std::enable_shared_from_this<SpacedSender> {
    asio::steady_timer timer;
    Session::Ptr session;
    std::vector<std::vector<uint8_t>> chunks;
    size_t idx = 0;
    int gapMs;
    SpacedSender(asio::io_context& io, Session::Ptr s,
                 std::vector<std::vector<uint8_t>> c, int g)
        : timer(io), session(std::move(s)), chunks(std::move(c)), gapMs(g) {}
    std::function<void()> onDone;
    void run() {
        if (idx >= chunks.size()) { if (onDone) { auto f = onDone; onDone = nullptr; f(); } return; }
        LOG_DEBUG("GAME", "drip chunk " + std::to_string(idx) + "/" +
                  std::to_string(chunks.size()) + " " + std::to_string(chunks[idx].size()) + "B");
        session->send(chunks[idx]);
        ++idx;
        if (idx >= chunks.size()) { if (onDone) { auto f = onDone; onDone = nullptr; f(); } return; }
        auto self = shared_from_this();
        timer.expires_after(std::chrono::milliseconds(gapMs));
        timer.async_wait([self](std::error_code ec) { if (!ec) self->run(); });
    }
};

}  // namespace

void GameServer::sendPlayerData(Session::Ptr session) {
    const auto ioT0 = std::chrono::steady_clock::now();
    LOG_INFO("GAME", "Sending player data to " + session->remoteAddress() +
             " (account=" + std::to_string(session->accountId) + ")");

    auto& db = Database::instance();

    auto chars = db.queryPrepared(
        "SELECT c.id, c.name, c.level, c.experience, c.gold, c.cash, c.wins, c.losses, "
        "COALESCE(c.total_races, 0) AS total_races, "
        "COALESCE(c.playtime_minutes, 0) AS playtime_minutes, "
        "COALESCE(c.license_class, 0) AS license_class, "
        "COALESCE(c.rank_points, 0) AS rank_points, "
        "COALESCE(c.equipped_driver_id, 1) AS equipped_driver_id, "
        "COALESCE(c.tutorial_completed, 0) AS tutorial_completed, "
        "COALESCE(c.is_gm, 0) AS is_gm, "
        "COALESCE(c.pendant_key, 0) AS pendant_key, "
        "COALESCE(a.gm_level, 0) AS gm_level, "
        "COALESCE(a.invite_opt_out, 0) AS invite_opt_out, "
        "COALESCE(UNIX_TIMESTAMP(c.muted_until), 0) AS muted_until "
        "FROM characters c "
        "LEFT JOIN accounts a ON a.id = c.account_id "
        "WHERE c.account_id = ? LIMIT 1",
        {static_cast<int32_t>(session->accountId)}
    );
    
    PlayerData player;
    player.level = 1;
    player.gold = 1000;
    
    // creation popup 0x03 needs catalogues sent first or sub 473730 random preview pick divides by zero
    const bool creating = chars.empty();
    const bool catalogs = !session->catalogsSent;
    if (creating) {
        LOG_INFO("GAME", "account " + std::to_string(session->accountId) +
                 " has no character, catalogues then the creation popup");
        session->characterId = 0;
        session->characterName.clear();
    }

    if (!creating) {
    player.id = std::stoi(chars[0]["id"]);
    player.accountId = session->accountId;
    player.ticketToken = session->sessionToken;
    player.name = chars[0]["name"];
    player.level = std::stoi(chars[0]["level"]);
    player.xp = std::stoi(chars[0]["experience"]);
    player.gold = std::stoi(chars[0]["gold"]);
    player.cash = std::stoi(chars[0]["cash"]);
    player.wins = std::stoi(chars[0]["wins"]);
    player.losses = std::stoi(chars[0]["losses"]);
    player.totalRaces = std::stoi(chars[0]["total_races"]);
    player.playtimeMinutes = std::stoi(chars[0]["playtime_minutes"]);
    player.licenseClass = std::stoi(chars[0]["license_class"]);
    player.rankPoints = std::stoi(chars[0]["rank_points"]);
    player.driverId = std::stoi(chars[0]["equipped_driver_id"]);
    player.tutorialCompleted = chars[0]["tutorial_completed"] == "1";
    player.isGM = chars[0]["is_gm"] == "1";
    
    session->characterId = player.id;
    session->gmLevel = static_cast<uint8_t>(std::stoi(chars[0]["gm_level"]));
    session->mutedUntil = std::stoll(chars[0]["muted_until"]);
    // the stored option 11 stands until the client reports its own on C2S 0x0130
    session->inviteOptOut.store(chars[0]["invite_opt_out"] == "1");
    // the worn pendant rides the profile blob at 0x4C4 as sub 479080 copies it
    player.pendantKey = std::stoi(chars[0]["pendant_key"]);
    // fill UTF sixteen name else broadcasts carry empty
    session->characterName.clear();
    session->characterName.reserve(player.name.size());
    for (char c : player.name) {
        session->characterName.push_back(static_cast<char16_t>(static_cast<uint8_t>(c)));
    }

    LOG_INFO("GAME", "Character loaded: " + player.name +
             " (ID=" + std::to_string(player.id) +
             " Level=" + std::to_string(player.level) +
             " Gold=" + std::to_string(player.gold) +
             " Cash=" + std::to_string(player.cash) +
             " Wins=" + std::to_string(player.wins) + ")");
    }

    // the claim belongs to the io thread a second login must never repeat 0xBE
    session->catalogsSent = true;

    // every read below used to run here and it was the worst stall on the only io thread
    auto frames = std::make_shared<LoginBurstFrames>();
    auto self = this;
    auto job = [self, session, player, creating, catalogs, frames]() {
        const auto dbT0 = std::chrono::steady_clock::now();
        self->collectLoginBurst(session, player, creating, catalogs, *frames);
        frames->dbMs = msSince(dbT0);
        self->postIo([self, session, player, creating, frames]() {
            self->dripLoginBurst(session, player, creating, *frames);
        });
    };

    LOG_INFO("GAME", "login burst io phase " + fmtMs(msSince(ioT0)) + " ms account " +
             std::to_string(session->accountId));

    // postDb refuses when the pool is off and then the whole burst runs here as it always did
    if (!postDb(session->id(), job)) job();
}

void GameServer::collectLoginBurst(const Session::Ptr& session, const PlayerData& playerIn,
                                   bool creating, bool catalogs, LoginBurstFrames& out) {
    PlayerData player = playerIn;
    auto& db = Database::instance();
    std::vector<Packet> ownedBurst;

    if (!creating) {
    // a driver granted by shop gacha or mission has empty BODYSET slots so repair them before buildLoginBurst runs
    for (const char* part : {"body", "face", "head"}) {
        const std::string col = std::string("acc_") + part;
        db.executePrepared(
            "UPDATE owned_character oc "
            "JOIN drivers d ON d.id = oc.base_key "
            "JOIN def_kart_skin s ON s.category = 2 "
            "  AND s.name LIKE CONCAT(d.name, '\_char\_" + std::string(part) + "\_%') "
            "SET oc." + col + " = s.skin_key "
            "WHERE oc.character_id = ? AND oc." + col + " = 0",
            {static_cast<int32_t>(player.id)});
    }

    // an owned chassis goes on mode 3 with its slot and basic set before 0x001C reads the karts
    if (CustomCarPackets::syncFactoryLoadouts(static_cast<int32_t>(player.id)))
        CarCraftHandler::invalidateCustomCar(static_cast<int32_t>(player.id));

    // owned tables must settle before 0x000A reads the two selection columns and containers must ride the same burst
    m_inventoryHandler.buildLoginBurst(static_cast<int32_t>(player.id), ownedBurst);

    // reconcile can repoint the driver column so reread what the blob is about to carry
    auto driverNow = db.queryPrepared(
        "SELECT COALESCE(equipped_driver_id, 0) AS d FROM characters WHERE id = ? LIMIT 1",
        {static_cast<int32_t>(player.id)});
    if (!driverNow.empty() && !driverNow[0]["d"].empty()) {
        player.driverId = std::stoi(driverNow[0]["d"]);
    }

    // owned kart is the only kart truth the selection column names the instance
    auto equippedVeh = db.queryPrepared(
        "SELECT k.id, k.base_key FROM owned_kart k JOIN characters c ON c.id = k.character_id "
        "WHERE k.character_id = ? AND c.selected_kart_instance_id = k.id LIMIT 1",
        {static_cast<int32_t>(player.id)}
    );
    if (equippedVeh.empty()) {
        equippedVeh = db.queryPrepared(
            "SELECT id, base_key FROM owned_kart WHERE character_id = ? "
            "ORDER BY active_flag DESC, id ASC LIMIT 1",
            {static_cast<int32_t>(player.id)}
        );
    }
    if (!equippedVeh.empty()) {
        player.vehicleId = std::stoi(equippedVeh[0]["id"]);
        player.vehicleTemplateId = std::stoi(equippedVeh[0]["base_key"]);
        LOG_DEBUG("GAME", "Equipped kart: ID=" + std::to_string(player.vehicleId) +
                  " Template=" + std::to_string(player.vehicleTemplateId));
    }
    }

    // client packet buffer is 0x2000 so cut the burst into chunks and drip them spaced apart see SpacedSender
    std::vector<std::vector<uint8_t>> chunks;
    std::vector<uint8_t> cur;
    const size_t CHUNK_LIMIT = dripChunk();
    // order not size matters catalogues then player then post ack data see docs packets LOGIN ORDER md
    std::vector<Packet> burst;
    auto appendPkt = [&burst](Packet p) { burst.push_back(std::move(p)); };

    auto phaseOf = [](uint16_t op) -> int {
        switch (op) {
            case 0x00BE: return 0;
            case 0x00C6: return 1;
            case 0x00BF: return 2;
            case 0x00C0: return 3;
            case 0x00C2: return 4;
            case 0x0103: return 5;
            case 0x00C1: return 6;
            case 0x0108: return 7;
            case 0x010C: return 8;
            case 0x0119: return 5;    // pendant definitions right after the pets on their wire
            case 0x00C4: return 9;
            case 0x00C3: return 10;
            case 0x00F1: return 11;
            case 0x0007: return 12;
            case 0x001B: return 13;
            case 0x001C: return 14;
            case 0x001D: return 15;
            case 0x0104: return 16;
            case 0x011A: return 16;
            case 0x0123: return 17;   // owned pendants ride with the player worn one lands between 0x104 and 0x10D
            case 0x010D: return 18;
            case 0x000E: return 19;
            default:     return 20;   // ours and theirs both trail extras here
        }
    };

    auto flushBurst = [&burst, &chunks, &cur, CHUNK_LIMIT, &phaseOf]() {
        // a replayed reference burst is already in the right order so leave it alone
        if (!std::getenv("KNC_REPLAY_BURST"))
        std::stable_sort(burst.begin(), burst.end(),
                         [&phaseOf](const Packet& a, const Packet& b) {
                             return phaseOf(a.opcode()) < phaseOf(b.opcode());
                         });
        for (const Packet& p : burst) {
            auto d = p.serialize();
            if (!cur.empty() && cur.size() + d.size() > CHUNK_LIMIT) {
                chunks.push_back(std::move(cur));
                cur.clear();
            }
            cur.insert(cur.end(), d.begin(), d.end());
        }
        burst.clear();
    };

    // 0xBE wipes twenty containers and drags the 0xC6 price table with it or every shop tile draws with no price

    if (catalogs) m_shopHandler.sendLoginCatalogs(session, this, &out.preBurst);

    // 0xC5 licence test definitions FUN 00453330 resolves the tutorial licence there so it must carry real rows
    if (catalogs && !lazyCatalogs()) {
        const auto licenseDefs = MissionPackets::loadLicenseTestDefs();
        for (const auto& d : licenseDefs) {
            appendPkt(MissionPackets::licenseTestDefinition(d));
        }
        LOG_INFO("GAME", "sent " + std::to_string(licenseDefs.size()) +
                 " licence test definitions");
    }

    // 0x010C room object catalogue must precede 0x010D owned rows or sub 488300 Floor lookup reads a wild pointer
    m_roomCraftHandler.pushObjectCatalog(session, &out.preBurst);
    m_roomCraftHandler.pushOwnedInstances(session, &out.preBurst);

    // pendants 0x119 one definition each 0x11A one owned each see 037 pendants sql
    {
        auto& pdb = Database::instance();
        auto defs = pdb.queryPrepared(
            // sub 46EDB0 counts hidden rows in its hit test so a hidden one goes last
            "SELECT pendant_key, icon_base, title_key, info_key, visible "
            "FROM pendant_def ORDER BY visible DESC, pendant_key LIMIT 64", {});
        for (const auto& r : defs) {
            if (!catalogs) break;
            appendPkt(PacketBuilder::pendantDefinition(
                std::stoi(r.at("visible")), std::stoi(r.at("pendant_key")),
                r.at("icon_base"), r.at("title_key"), r.at("info_key")));
        }
        // a level or a race count reached before this build still earns its pendant
        if (!creating) ProgressionHandler::grantEarnedPendants(static_cast<uint32_t>(player.id));
        auto owned = creating ? std::vector<std::map<std::string, std::string>>{} : pdb.queryPrepared(
            "SELECT o.pendant_key FROM owned_pendant o WHERE o.character_id = ? "
            "ORDER BY o.pendant_key LIMIT 64",
            {std::to_string(player.id)});
        for (const auto& r : owned) {
            // sub 451250 erases by instance so each row carries its own id the key is unique per character
            const uint32_t key = static_cast<uint32_t>(std::stoul(r.at("pendant_key")));
            appendPkt(PacketBuilder::entitySimple(false, static_cast<int32_t>(pendantInstanceId(key)),
                                                  static_cast<int32_t>(key)));
        }
        // the equipped one 0x123 with the key or 0 sent right after 0x104 in the player phase
        if (!creating) {
            auto eq = pdb.queryPrepared(
                "SELECT COALESCE(pendant_key, 0) AS p FROM characters WHERE id = ? LIMIT 1",
                {std::to_string(player.id)});
            Packet worn = Packet::fromCmdFull(CMD::C_TITLE_EQUIP);
            worn.writeInt32(eq.empty() ? 0 : std::stoi(eq[0].at("p")));
            appendPkt(std::move(worn));
        }
        LOG_INFO("GAME", "pendants " + std::to_string(defs.size()) + " defs, " +
                 std::to_string(owned.size()) + " owned for char " + std::to_string(player.id));
    }

    if (!creating) appendPkt(PacketBuilder::sessionConfirm(session->characterId, player));
    if (!creating) {
        // legacy builder writes wins and losses where client reads exp floor and next so a fresh char needs nonzero
        Packet statsPkt;
        if (ProgressionHandler::buildStatsRefresh(static_cast<uint32_t>(player.id), statsPkt)) {
            appendPkt(std::move(statsPkt));
        } else {
            LOG_WARN("GAME", "progression stats unavailable falling back to the legacy 0x0A builder");
            appendPkt(PacketBuilder::connectionOkWithPlayer(player));
        }
    }

    // content type catalogs must precede inventory or lookups hit empty catalog unk 80E680 driver catalog and crash
    if (catalogs) {
    auto skinRows = db.queryPrepared(
        "SELECT skin_key, name, category, COALESCE(display_name, '') AS display_name "
        "FROM def_kart_skin ORDER BY skin_key LIMIT 1024", {});

    // every catalogue tile needs its price option list or the draw dereferences null
    std::map<std::pair<int32_t, int32_t>, std::vector<uint32_t>> shopOptions;
    for (const auto& so : db.queryPrepared(
             "SELECT category, base_key, price_key FROM shop_option ORDER BY category, "
             "base_key, slot", {})) {
        const int32_t cat = std::stoi(so.at("category"));
        const int32_t key = std::stoi(so.at("base_key"));
        auto& v = shopOptions[{cat, key}];
        if (v.size() < 4) v.push_back(static_cast<uint32_t>(std::stoul(so.at("price_key"))));
    }
    // the tile draws three price rows one day seven days permanent pick by what shop price says the key is
    std::map<uint32_t, std::pair<int32_t, int32_t>> priceUnit;
    for (const auto& pr : db.queryPrepared("SELECT price_key, unit_type, unit_amount FROM shop_price", {})) {
        priceUnit[static_cast<uint32_t>(std::stoul(pr.at("price_key")))] =
            { std::stoi(pr.at("unit_type")), std::stoi(pr.at("unit_amount")) };
    }
    auto optionsFor = [&shopOptions, &priceUnit](int32_t cat, int32_t key) {
        auto it = shopOptions.find({cat, key});
        if (it == shopOptions.end()) return std::vector<uint32_t>{};
        const std::vector<uint32_t>& all = it->second;
        static const std::pair<int32_t, int32_t> kWanted[3] = { {1, 1}, {1, 7}, {0, 0} };
        std::vector<uint32_t> out;
        for (const auto& want : kWanted) {
            for (uint32_t k : all) {
                auto pu = priceUnit.find(k);
                if (pu != priceUnit.end() && pu->second == want) { out.push_back(k); break; }
            }
        }
        if (out.empty()) {
            // keys with no price row use whatever the seed has three at most
            for (uint32_t k : all) { if (out.size() < 3) out.push_back(k); }
        }
        return out;
    };

    auto driverRows = db.queryPrepared(
        "SELECT id, name, COALESCE(display_name, '') AS display_name, "
        "COALESCE(info_key, '') AS info_key, COALESCE(creation_pick, 0) AS creation_pick "
        "FROM drivers WHERE COALESCE(is_enabled, 1) = 1 ORDER BY id", {});
    for (const auto& d : driverRows) {
        int32_t did = std::stoi(d.at("id"));
        // slot keys name BODYSET meshes O BODY O FACE O HEAD O GLASS O BACK zero leaves the bone bare
        const std::string asset = driverBodyAsset(d.at("name"));
        // empty cosmetic slots are -1 not 0 a 0 makes the model builder resolve part key 0 and choke
        std::array<int32_t, 5> slots{-1, -1, -1, -1, -1};
        for (const auto& sk : skinRows) {
            const std::string& n = sk.at("name");
            if (n.rfind(asset + "_char_body_", 0) == 0) slots[0] = std::stoi(sk.at("skin_key"));
            if (n.rfind(asset + "_char_face_", 0) == 0) slots[1] = std::stoi(sk.at("skin_key"));
            if (n.rfind(asset + "_char_head_", 0) == 0) slots[2] = std::stoi(sk.at("skin_key"));
        }
        // the garage draws str2 raw so send the finished display name instead of a blank title key
        std::string label = d.count("display_name") ? d.at("display_name") : std::string();
        if (label.empty()) label = d.at("name");
        // empty option list crashes the shop tile renderer real keys live in shop option seeded by migration 012
        const char* dp = std::getenv("KNC_DRIVER_PRICES");
        const std::vector<uint32_t> driverPrices =
            (dp && dp[0] == '0') ? std::vector<uint32_t>{} : optionsFor(0, did);
        const std::string dInfo = d.count("info_key") ? d.at("info_key") : std::string();
        const bool pick = d.count("creation_pick") && d.at("creation_pick") == "1";
        appendPkt(PacketBuilder::driverCatalog(did, asset, label, slots, driverPrices, dInfo, pick));
    }
    // 0xC0 vehicle catalog send only owned templates at login the lobby needs just the equipped kart
    auto vehTemplates = db.queryPrepared(
        // publish the whole catalogue not owned rows only or sub 44F510 shop Car tab lists just one kart
        "SELECT id, name, COALESCE(is_factory_car, 0) AS is_factory_car, "
        "COALESCE(title_key, '') AS title_key, COALESCE(info_key, '') AS info_key, "
        "COALESCE(stat_speed,50) AS stat_speed, COALESCE(stat_accel,50) AS stat_accel, "
        "COALESCE(stat_handling,50) AS stat_handling, COALESCE(stat_drift,40) AS stat_drift, "
        "COALESCE(stat_boost,30) AS stat_boost "
        "FROM vehicle_templates WHERE COALESCE(is_enabled, 1) = 1 "
        "ORDER BY id LIMIT 60", {});
    // theme then track catalogue must arrive before sub 453330 tutorial or licence stage looks a track up
    if (!lazyCatalogs()) {
        auto themes = SpawnPackets::themeCatalogAll();
        for (const auto& th : themes) appendPkt(SpawnPackets::themeCatalogEntry(th));
        auto tracks = SpawnPackets::trackCatalogAll();
        for (const auto& tr : tracks) appendPkt(SpawnPackets::trackCatalogEntry(tr));
        LOG_INFO("GAME", "sent " + std::to_string(themes.size()) + " themes and " +
                 std::to_string(tracks.size()) + " tracks");
    }

    // shop part tab filter 0x418C80 maps tabs to slots 0-6 and 8 slot 7 stays out of every tab
    constexpr int32_t kUiCatHidden = 7;
    // 0x490A70 and 0x4A5ED0 read factory parts from 0x108 only twin keys 2000-2004 shadow common char head 001-005
    const char* envUi = std::getenv("KNC_PART_UICAT");
    const bool useUiCat = !envUi || std::string(envUi) != "0";
    LOG_INFO("GAME", std::string("catalog toggles uicat=") + (useUiCat ? "1" : "0"));

    // colours ship in the same container before the karts that name them tab id comes from def kart skin category
    std::map<std::string, int32_t> driverIdByName;
    for (const auto& dr : db.queryPrepared("SELECT id, LOWER(name) AS lname FROM drivers", {})) {
        driverIdByName[dr.at("lname")] = std::stoi(dr.at("id"));
    }
    for (const auto& sd : skinRows) {
        const int32_t cat = std::stoi(sd.at("category"));
        const std::string& n = sd.at("name");
        int32_t uiCat = cat;
        if (cat == 2) {
            // tab ids off chibikart 0xC2 rows body 2 face 3 head and cap 4 glass 5 back and bag 6
            auto has = [&n](const char* part) { return n.find(part) != std::string::npos; };
            if (has("_char_body_") || has("_char_bady_"))   uiCat = 2;
            else if (has("_char_face_"))                    uiCat = 3;
            else if (has("_char_head_") || has("_char_cap_")) uiCat = 4;
            else if (has("_char_glass_"))                   uiCat = 5;
            else if (has("_char_back_") || has("_char_bag_")) uiCat = 6;
            else                                            uiCat = kUiCatHidden;
        }
        // str1 stays the asset str2 resolves through sub 4E1B70 def trans index str3 is the info text
        std::string label = sd.count("display_name") ? sd.at("display_name") : std::string();
        if (label.empty()) label = n;
        std::string info = label;
        if (label.rfind("PART_", 0) == 0 && label.size() > 6 &&
            label.compare(label.size() - 6, 6, "_TITLE") == 0) {
            info = label.substr(0, label.size() - 6) + "_INFO";
        }
        const int32_t skinKey = std::stoi(sd.at("skin_key"));
        int32_t driverRestrict = -1;
        if (cat == 2) {
            const size_t us = n.find("_char_");
            if (us != std::string::npos) {
                std::string drv = n.substr(0, us);
                for (auto& ch : drv) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                auto it = driverIdByName.find(drv);
                if (it != driverIdByName.end()) driverRestrict = it->second;
            }
        }
        const std::vector<uint32_t> skinPrices = optionsFor(3, skinKey);
        appendPkt(PacketBuilder::partCatalog(skinKey, useUiCat ? uiCat : 0, n, label, info,
                                             useUiCat ? 0 : cat, skinPrices,
                                             driverRestrict, cat != 2, !skinPrices.empty()));
    }

    for (const auto& t : vehTemplates) {
        int32_t tid = std::stoi(t.at("id"));
        const std::string& modelName = t.at("name");

        // every burst kart row names the stock paint and plate a factory car takes parts from its preset
        std::array<int32_t, 8> defaultParts{};
        defaultParts[0] = kDefaultPaintKey;
        defaultParts[1] = kDefaultPlateKey;
        std::vector<uint32_t> kartPrices = optionsFor(1, tid);
        // a chassis sells one gold option the full durability bar the grant puts it on mode 3 whatever key
        const bool chassis = factoryFlag(t);
        if (chassis && kartPrices.size() > 1) kartPrices = {kartPrices.front()};
        const std::string titleKey = t.count("title_key") ? t.at("title_key") : std::string();
        const std::string infoKey   = t.count("info_key")  ? t.at("info_key")  : std::string();
        // the 17 floats go in wire order float k lands at car 0x3448 plus 4 k see migration 059
        const std::array<float, 17> stats = kartWireStats(t);
        appendPkt(PacketBuilder::vehicleCatalog(tid, modelName, defaultParts, kartPrices,
                                                chassis, titleKey, infoKey, stats, {},
                                                chassis ? CustomCarPackets::FACTORY_START_DURABILITY : 0));
    }
    LOG_INFO("GAME", "Sent catalogs " + std::to_string(driverRows.size()) + " drivers " +
             std::to_string(vehTemplates.size()) + " vehicles");
    }   // catalogs

    // only publisher of 0x001B dword 1A56BD0 and 0x001C dword 1A576D8 sub 42A050 walks a second publish wipes them
    for (auto& p : ownedBurst) appendPkt(std::move(p));
    ownedBurst.clear();

    // not seating here sub 40D650 and sub 40CC90 bail until sub 40F7E0 arms the gate 0xD9 is what lobby needs
    if (!creating) {
        RoomMemberWire me = buildRoomMember(static_cast<int32_t>(player.id),
                                            session->characterName, 0, 0, false);
        appendPkt(PacketBuilder::roomLoadoutUpdate(static_cast<int32_t>(player.id),
                                                   me.character, me.kart, me.customCar));
    }

    // 0xF1 wall clock chibikart ships it in the login burst so the client time is right
    appendPkt(PacketBuilder::clockSync());

    // 0x96 gift inbox record layout captured from chibikart id category sender name date time subject
    if (!creating) {
        auto giftRows = db.queryPrepared(
            "SELECT g.id, g.category, g.sender_id, g.message, "
            "DATE_FORMAT(g.sent_at, '%Y-%m-%d') AS d, DATE_FORMAT(g.sent_at, '%H:%i:%s') AS t, "
            "COALESCE(sc.name, 'System') AS sender_name "
            "FROM gift_log g LEFT JOIN characters sc ON sc.id = g.sender_id "
            "WHERE (g.target_id = ? OR g.target_name = ?) AND g.read_flag = 0 "
            "ORDER BY g.id DESC LIMIT 30",
            {static_cast<int32_t>(player.id), player.name});
        std::vector<std::array<uint8_t, 0xD4>> inbox;
        for (const auto& g : giftRows) {
            inbox.push_back(PacketBuilder::giftRecord(
                static_cast<int32_t>(std::stoll(g.at("id")) & 0x7fffffff),
                std::stoi(g.at("category")), std::stoi(g.at("sender_id")),
                g.at("sender_name"), g.at("d"), g.at("t"), g.at("message")));
        }
        // the mailbox is container B reached by 0x95 and 0x97 which share one handler
        appendPkt(PacketBuilder::giftInboxListSent(inbox));

        // 0x96 holds mail already taken this one holds what is still to be taken and 0x9C moves a row across
        auto readRows = db.queryPrepared(
            "SELECT g.id, g.category, g.sender_id, g.message, "
            "DATE_FORMAT(g.sent_at, '%Y-%m-%d') AS d, DATE_FORMAT(g.sent_at, '%H:%i:%s') AS t, "
            "COALESCE(sc.name, 'System') AS sender_name "
            "FROM gift_log g LEFT JOIN characters sc ON sc.id = g.sender_id "
            "WHERE (g.target_id = ? OR g.target_name = ?) AND g.read_flag = 1 "
            "ORDER BY g.id DESC LIMIT 30",
            {static_cast<int32_t>(player.id), player.name});
        std::vector<std::array<uint8_t, 0xD4>> taken;
        for (const auto& g : readRows) {
            taken.push_back(PacketBuilder::giftRecord(
                static_cast<int32_t>(std::stoll(g.at("id")) & 0x7fffffff),
                std::stoi(g.at("category")), std::stoi(g.at("sender_id")),
                g.at("sender_name"), g.at("d"), g.at("t"), g.at("message")));
        }
        appendPkt(PacketBuilder::giftInboxList(taken));       // 0x96 the taken ones
        if (!inbox.empty() || !taken.empty())
            LOG_INFO("GAME", "gifts " + std::to_string(inbox.size()) + " waiting, " +
                     std::to_string(taken.size()) + " taken");
    }

    // catalogs chibikart ships at login that we never did empty counts are valid and add under 150 bytes
    {
        // no 0x95 here shares 0x97 handler 0xC1 record icon stem at 0x14 title key at 0x35 written by itemDef
        for (auto& p : PartStatPackets::itemDefTable(PartStatPackets::loadItemDefs()))
            appendPkt(p);
        // no empty list stubs on 0xC3 0xC5 0x10C 0x10D sub 478B43 reads a fixed record a short frame drops it
        for (auto& p : GachaPetPackets::petCatalog(GachaPetPackets::loadPetDefs()))
            appendPkt(p);
        // the owned pet list already rode with ownedBurst a second empty 0x104 would wipe it on the client
        if (!PacketBuilder::burstHasOpcode(burst, CMD::S_ENTITY_DATA_260))
            appendPkt(PacketBuilder::entityList260());
        // no 0x107 at login chibikart never sends it there it belongs to car craft on screen entry only
    }

    // quest mode 0x00F3 defs after the 0xBE wipe and the 0x00F4 rows the stage 22 init reads
    {
        auto questFrames = ScenarioHandler::loginFrames(creating ? 0u : static_cast<uint32_t>(player.id), catalogs);
        for (auto& p : questFrames) appendPkt(std::move(p));
    }

    if (!creating) db.executePrepared("UPDATE characters SET last_played = NOW() WHERE id = ?", {static_cast<int32_t>(player.id)});

    if (creating) {
        // last after every catalogue the phase sort puts ahead of it
        appendPkt(Packet(CMD::S_SET_GAME_VAR));
    }

    // no padding 0x20 is sub 4797C0 RoomFull which parses a large struct pad bytes desync the reader

    // KNC REPLAY BURST replays a working server's login burst byte for byte KNC REPLAY LIMIT bisects to the breaking frame
    {
        const char* rp = std::getenv("KNC_REPLAY_BURST");
        if (rp && *rp) {
            // the server builds on linux fopen s is a windows extension
            FILE* f = std::fopen(rp, "rb");
            if (f) {
                char magic[4] = {};
                uint32_t n = 0;
                if (std::fread(magic, 1, 4, f) == 4 && memcmp(magic, "KNCB", 4) == 0 &&
                    std::fread(&n, 4, 1, f) == 1) {
                    // an empty variable is not a limit of zero it means no limit
                    const char* lim = std::getenv("KNC_REPLAY_LIMIT");
                    const uint32_t cap = (lim && *lim && std::atoi(lim) > 0)
                                       ? static_cast<uint32_t>(std::atoi(lim)) : n;
                    // KNC REPLAY OURS lists opcodes kept from our own burst while the rest comes from the reference
                    std::set<uint16_t> keepOurs;
                    if (const char* ko = std::getenv("KNC_REPLAY_OURS")) {
                        const char* q = ko;
                        while (*q) {
                            char* end = nullptr;
                            const long v = std::strtol(q, &end, 0);
                            if (end == q) break;
                            if (v > 0) keepOurs.insert(static_cast<uint16_t>(v));
                            q = end;
                            while (*q == ',' || *q == ' ') ++q;
                        }
                    }
                    std::vector<Packet> mine;
                    if (!keepOurs.empty()) {
                        for (Packet& p : burst)
                            if (keepOurs.count(p.opcode())) mine.push_back(std::move(p));
                    }
                    burst.clear();
                    uint32_t sent = 0;
                    for (uint32_t i = 0; i < n && sent < cap; ++i) {
                        uint16_t op = 0, len = 0;
                        if (std::fread(&op, 2, 1, f) != 1 || std::fread(&len, 2, 1, f) != 1) break;
                        std::vector<uint8_t> pay(len);
                        if (len && std::fread(pay.data(), 1, len, f) != len) break;
                        if (keepOurs.count(op)) continue;   // ours goes in instead
                        Packet p = Packet::fromCmdFull(op);
                        if (len) p.writeBytes(pay.data(), pay.size());
                        burst.push_back(std::move(p));
                        ++sent;
                    }
                    for (Packet& p : mine) burst.push_back(std::move(p));
                    LOG_WARN("GAME", "REPLAY burst, " + std::to_string(sent) +
                                     " reference frames of " + std::to_string(n) +
                                     ", ours kept for " + std::to_string(keepOurs.size()) +
                                     " opcodes");
                }
                std::fclose(f);
            } else {
                LOG_ERROR("GAME", std::string("KNC_REPLAY_BURST cannot open ") + rp);
            }
        }
    }

    // everything collected now put it in their order and cut it into chunks
    flushBurst();
    if (!cur.empty()) chunks.push_back(std::move(cur));

    // no compensation here the client buffer overwrite is clamped in the dinput8 proxy instead see tools clientpatch spy onArm
    size_t total = 0;
    for (const auto& c : chunks) total += c.size();
    size_t nChunks = chunks.size();
    // wire self check walk every chunk like ParsePacketsLoop does a lying length wedges the reader for good
    for (size_t ci = 0; ci < chunks.size(); ++ci) {
        const auto& buf = chunks[ci];
        size_t off = 0; int frames = 0;
        while (off + 8 <= buf.size()) {
            const uint16_t len = static_cast<uint16_t>(buf[off] | (buf[off + 1] << 8));
            const uint16_t op  = static_cast<uint16_t>(buf[off + 2] | (buf[off + 3] << 8));
            if (len + 8u >= 0x2000u) {
                LOG_ERROR("WIRE", "chunk " + std::to_string(ci) + " frame " + std::to_string(frames) +
                          " opcode 0x" + toHex16(op) + " len " + std::to_string(len) + " exceeds 0x2000 client sets error -4");
                break;
            }
            if (off + 8 + len > buf.size()) {
                LOG_ERROR("WIRE", "TRUNCATED frame chunk " + std::to_string(ci) + " frame " + std::to_string(frames) +
                          " opcode 0x" + toHex16(op) + " claims " + std::to_string(len) +
                          " but only " + std::to_string(buf.size() - off - 8) + " bytes remain, client reader wedges here and drops every packet after");
                break;
            }
            off += 8 + len; ++frames;
        }
        if (off != buf.size()) {
            LOG_ERROR("WIRE", "chunk " + std::to_string(ci) + " does not end on a frame boundary, consumed " +
                      std::to_string(off) + " of " + std::to_string(buf.size()));
        } else {
            std::string head;
            const size_t hn = buf.size() < 64 ? buf.size() : 64;
            for (size_t i = 0; i < hn; ++i) { char t[4]; snprintf(t, sizeof(t), "%02X ", buf[i]); head += t; }
            LOG_INFO("WIRE", "chunk " + std::to_string(ci) + " clean " + std::to_string(frames) +
                     " frames " + std::to_string(buf.size()) + " bytes head " + head);
        }
    }

    out.chunks = std::move(chunks);
    out.totalBytes = total;
    out.chunkCount = nChunks;
}

void GameServer::dripLoginBurst(const Session::Ptr& session, const PlayerData& player,
                                bool creating, LoginBurstFrames& frames) {
    const auto ioT0 = std::chrono::steady_clock::now();

    // the shop and room craft frames keep their own framing so they lead exactly as before
    for (const Packet& p : frames.preBurst) session->send(p);

    auto sender = std::make_shared<SpacedSender>(m_ioContext, session,
                                                 std::move(frames.chunks), dripGapMs());
    // S2C 11 ShowLobby calls sub 4538B0 and bails when g Dialog Code is 2 so the ack lands last alone
    const bool tut = player.tutorialCompleted;
    auto self = this;
    sender->onDone = [self, session, tut, creating]() {
        // the creation pass ends on the 0x03 popup nothing may follow it until the client sends 0x04
        if (creating) {
            session->m_burstFilter = false;
            LOG_INFO("GAME", "creation popup is up for account " + std::to_string(session->accountId) +
                     ", waiting for the 0x04");
            return;
        }
        // bisect proved none of these three desync the stream set KNC SKIP TAIL to strip them again
        if (!getenv("KNC_SKIP_TAIL")) {
            self->m_socialHandler.pushSocialLists(session, self);
            self->m_carCraftHandler.sendLoginSnapshot(session);
            QuestHandler::onCharacterEnter(session, self);
        }
        // proved in game 0x11 lands on stage 5 the menu so the IDA labels are off by one
        Packet ack(tut ? static_cast<uint16_t>(CMD::S_SHOW_LOBBY)
                       : static_cast<uint16_t>(CMD::S_UI_STATE_14));
        auto raw = ack.serialize();
        std::string hex;
        for (uint8_t b : raw) { char t[4]; snprintf(t, sizeof(t), "%02X ", b); hex += t; }
        session->send(ack);
        LOG_INFO("GAME", "screen ack on the wire bytes=[" + hex + "] stage 8 expected");
        // the burst is over everything else may flow again
        session->m_burstFilter = false;
        // the client info probe and delayed ack keepalive two empty S2C frames the client answers on its own
        KeepaliveAnticheatHandler::armPostLogin(session);
        // notes that arrived while offline 0x83 so the inbox sorts itself
        self->m_socialHandler.pushNoteBacklog(session, self);
        // sub 408020 clears the grid while handling that ack so the rows must follow it never precede it
        if (tut) self->sendLobbyRoomList(session);
    };
    if (loginDelayMs() > 0) {
        auto hold = std::make_shared<asio::steady_timer>(m_ioContext);
        hold->expires_after(std::chrono::milliseconds(loginDelayMs()));
        hold->async_wait([hold, sender](const std::error_code& ec) { if (!ec) sender->run(); });
        LOG_INFO("GAME", "login burst held " + std::to_string(loginDelayMs()) + " ms");
    } else {
        sender->run();
    }
    LOG_INFO("GAME", "login burst pool " + fmtMs(frames.dbMs) + " ms io drip " +
             fmtMs(msSince(ioT0)) + " ms " + std::to_string(frames.totalBytes) +
             " bytes in " + std::to_string(frames.chunkCount) + " chunks for " + player.name);

    // owned containers already rode the burst above they must not be republished here
}

void GameServer::loadoutBlobs(const RoomPlayer& rp,
                              std::array<uint8_t, 0x2C>& character,
                              std::array<uint8_t, 0x38>& kart,
                              std::array<uint8_t, 0x3C>& customCar) {
    if (rp.isBot) {
        const RoomMemberWire m = buildBotMember(rp);
        character = m.character;
        kart      = m.kart;
        customCar = m.customCar;
        return;
    }
    const RoomMemberWire m = buildRoomMember(rp.characterId, rp.name, rp.slot, rp.team, rp.ready);
    character = m.character;
    kart      = m.kart;
    customCar = m.customCar;
}

void GameServer::sendDripped(const std::shared_ptr<Session>& session,
                             std::vector<Packet> packets) {
    if (!session || packets.empty()) return;

    // same limit the login burst uses two coalesced still sit under 0x2000
    const size_t CHUNK_LIMIT = dripChunk();
    std::vector<std::vector<uint8_t>> chunks;
    std::vector<uint8_t> cur;
    for (auto& p : packets) {
        auto d = p.serialize();
        if (!cur.empty() && cur.size() + d.size() > CHUNK_LIMIT) {
            chunks.push_back(std::move(cur));
            cur.clear();
        }
        cur.insert(cur.end(), d.begin(), d.end());
    }
    if (!cur.empty()) chunks.push_back(std::move(cur));

    // walk every chunk exactly like the client parse loop does a lying length loses everything behind it
    for (size_t ci = 0; ci < chunks.size(); ++ci) {
        const auto& buf = chunks[ci];
        size_t off = 0;
        int frames = 0;
        while (off + 8 <= buf.size()) {
            const uint16_t len = static_cast<uint16_t>(buf[off] | (buf[off + 1] << 8));
            const uint16_t op  = static_cast<uint16_t>(buf[off + 2] | (buf[off + 3] << 8));
            if (len + 8u >= 0x2000u) {
                LOG_ERROR("WIRE", "drip chunk " + std::to_string(ci) + " frame " +
                          std::to_string(frames) + " opcode 0x" + toHex16(op) + " len " +
                          std::to_string(len) + " exceeds 0x2000 client sets error -4");
                break;
            }
            if (off + 8 + len > buf.size()) {
                LOG_ERROR("WIRE", "drip chunk " + std::to_string(ci) + " frame " +
                          std::to_string(frames) + " opcode 0x" + toHex16(op) + " len " +
                          std::to_string(len) + " runs past the chunk end at " +
                          std::to_string(off));
                break;
            }
            off += 8 + len;
            ++frames;
        }
        if (off != buf.size()) {
            LOG_ERROR("WIRE", "drip chunk " + std::to_string(ci) + " ends mid frame, walked " +
                      std::to_string(off) + " of " + std::to_string(buf.size()));
        }
        LOG_INFO("WIRE", "drip chunk " + std::to_string(ci) + " " + std::to_string(frames) +
                 " frames " + std::to_string(buf.size()) + " bytes");
    }

    if (chunks.size() == 1) {
        session->send(chunks[0]);
        return;
    }
    auto sender = std::make_shared<SpacedSender>(m_ioContext, session, std::move(chunks), dripGapMs());
    sender->run();
}

}  // namespace knc
