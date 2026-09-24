/// the licence screen and the licence test pass the older 0xA9 to 0xAC routes had no client sender

#include "handlers/LicenseHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "packets/gen/ProgressionPackets.h"
#include "handlers/ProgressionHandler.h"
#include "packets/gen/MissionPackets.h"
#include "logging/Logger.h"
#include "util/PendantRules.h"

namespace knc {

namespace {

// key space 0-3 rookie 10-13 amateur 20-23 pro three tests per band plus a bonus
constexpr uint32_t kBandSize = 3;
constexpr uint32_t kBonusIndex = 3;
constexpr uint32_t kBandStride = 10;
constexpr uint8_t  kMaxGrade = 3;

uint32_t bandOf(uint32_t licenseKey) { return licenseKey / kBandStride; }

// grade is how many whole bands are done the client clamps it to three anyway
uint8_t gradeFor(int32_t charId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT license_key FROM char_license_progress WHERE char_id = ? AND passed = 1",
        {charId});
    uint32_t perBand[4] = {0, 0, 0, 0};
    for (const auto& r : rows) {
        const uint32_t key = static_cast<uint32_t>(std::stoul(r.at("license_key")));
        if (key % kBandStride == kBonusIndex) continue;
        const uint32_t band = bandOf(key);
        if (band < 4) ++perBand[band];
    }
    uint8_t grade = 0;
    for (uint32_t b = 0; b < 4 && perBand[b] >= kBandSize; ++b) grade = static_cast<uint8_t>(b + 1);
    return grade > kMaxGrade ? kMaxGrade : grade;
}

}  // namespace

void LicenseHandler::handleOpenLicenseScreen(Session::Ptr session, GameServer* server) {
    if (!session || session->characterId == 0) return;
    const int32_t charId = static_cast<int32_t>(session->characterId);

    // twelve definitions plus the list stay well under 0x2000 but the table can grow
    std::vector<Packet> burst;
    const auto defs = MissionPackets::loadLicenseTestDefs();
    for (const auto& d : defs) burst.push_back(MissionPackets::licenseTestDefinition(d));

    burst.push_back(MissionPackets::licenseProgressList(MissionPackets::loadLicenseProgress(charId)));

    // a grade already earned but never recorded lands here with the 0xA4 celebration before the ack
    {
        const uint8_t earned = gradeFor(charId);
        uint8_t stored = 0;
        auto row = Database::instance().queryPrepared(
            "SELECT COALESCE(license_class, 0) AS g FROM characters WHERE id = ? LIMIT 1", {charId});
        if (!row.empty()) stored = static_cast<uint8_t>(std::stoul(row[0].at("g")));
        if (earned > stored) {
            ProgressionPackets::setLicenceGrade(session->characterId, earned);
            burst.push_back(MissionPackets::licenseGradeUp(earned));
            LOG_INFO("LICENSE", "grade " + std::to_string(stored) + " to " + std::to_string(earned) +
                     " granted on screen open for char " + std::to_string(charId));
        }
    }
    burst.push_back(MissionPackets::licenseScreenAck());

    if (server) server->sendDripped(session, std::move(burst));
    else for (auto& p : burst) session->send(p);

    LOG_INFO("LICENSE", "license screen char " + std::to_string(charId) + " defs " +
             std::to_string(defs.size()) + " grade " + std::to_string(gradeFor(charId)));
}

void LicenseHandler::handleLicenseComplete(Session::Ptr session, Packet& packet,
                                           GameServer* server) {
    (void)server;
    if (!session || session->characterId == 0) return;
    const int32_t charId = static_cast<int32_t>(session->characterId);

    MissionPackets::LicenseSubmitReq req;
    if (!MissionPackets::parseLicenseTestSubmit(packet, req)) {
        LOG_WARN("LICENSE", "submit from char " + std::to_string(charId) + " did not parse");
        return;
    }

    auto& db = Database::instance();
    auto def = db.queryPrepared(
        "SELECT param_01, param_02, reward_item_type, reward_item_key, reward_item_count "
        "FROM license_test_def WHERE license_key = ? LIMIT 1",
        {static_cast<int32_t>(req.licenseKey)});
    if (def.empty()) {
        LOG_WARN("LICENSE", "submit key " + std::to_string(req.licenseKey) + " has no definition");
        return;
    }
    auto num = [&](const char* c) -> int32_t {
        auto it = def[0].find(c);
        return it == def[0].end() || it->second.empty() ? 0 : std::stoi(it->second);
    };

    // the two echoes in the payload are client input so the definition wins
    const int32_t expReward = num("param_01");
    const int32_t goldReward = num("param_02");

    auto prev = db.queryPrepared(
        "SELECT passed FROM char_license_progress WHERE char_id = ? AND license_key = ? LIMIT 1",
        {charId, static_cast<int32_t>(req.licenseKey)});
    const bool firstPass = prev.empty() || prev[0].at("passed") == "0";

    const uint8_t gradeBefore = gradeFor(charId);

    db.executePrepared(
        "REPLACE INTO char_license_progress (char_id, license_key, passed, unknown_08) "
        "VALUES (?, ?, 1, 0)",
        {charId, static_cast<int32_t>(req.licenseKey)});

    if (firstPass && (expReward != 0 || goldReward != 0)) {
        auto res = ProgressionPackets::awardExpAndGold(session->characterId, expReward, goldReward);
        if (!res.ok) {
            LOG_ERROR("LICENSE", "reward failed for key " + std::to_string(req.licenseKey) +
                      " char " + std::to_string(charId));
        }
    }

    MissionPackets::LicenseResultWire out;
    out.licenseKey = req.licenseKey;
    out.passed = 1;
    // exp and gold are already on the definition the client read so no currency tail is sent
    out.hasCurrency = false;

    const int32_t itemCount = num("reward_item_count");
    if (firstPass && itemCount > 0) {
        MissionPackets::RewardBlob blob;
        blob.type = static_cast<uint32_t>(num("reward_item_type"));
        const size_t want = MissionPackets::rewardBlobSize(blob.type);
        const uint32_t key = static_cast<uint32_t>(num("reward_item_key"));
        if (blob.type == 7 && want == 8 && key > 0) {
            // type 7 is the pendant sub 47C3C0 removes that key then appends the instance and key row
            db.executePrepared(
                "INSERT IGNORE INTO owned_pendant (character_id, pendant_key) VALUES (?, ?)",
                {charId, static_cast<int32_t>(key)});
            const auto row = pendantRow(key);
            blob.bytes.assign(row.begin(), row.end());
            out.hasItem = true;
            out.item = std::move(blob);
            LOG_INFO("LICENSE", "key " + std::to_string(req.licenseKey) + " grants pendant " +
                     std::to_string(key) + " to char " + std::to_string(charId));
        } else {
            // any other reward type needs a real inventory record we lack so it is dropped not desynced
            LOG_WARN("LICENSE", "reward type " + std::to_string(blob.type) + " on key " +
                     std::to_string(req.licenseKey) + " not sent, only type seven is built");
        }
    }

    session->send(MissionPackets::licenseTestResult(out));

    const uint8_t gradeAfter = gradeFor(charId);
    if (gradeAfter != gradeBefore) {
        ProgressionPackets::setLicenceGrade(session->characterId, gradeAfter);
        session->send(MissionPackets::licenseGradeUp(gradeAfter));
    }
    // licence exp can cross level 10 20 30 40 50 so those pendants go out now
    if (firstPass) ProgressionHandler::pushEarnedPendants(session);

    LOG_INFO("LICENSE", "test key " + std::to_string(req.licenseKey) + " passed char " +
             std::to_string(charId) + " grade " + std::to_string(gradeBefore) + " to " +
             std::to_string(gradeAfter) + (firstPass ? " first pass" : " repeat no pay"));
}

} // namespace knc
